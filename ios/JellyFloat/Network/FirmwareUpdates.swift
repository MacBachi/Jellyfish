import Foundation

/// A firmware image the app can send to a jelly: the .bin from a release and its signature
/// (tools/ota/ota_sign.py), for one build variant.
struct FirmwareImage: Identifiable, Equatable {
    let version: FirmwareVersion
    let variant: String          // "default", "ring39-GRB-tRGB", "ring94-GRB-tRGB", ...
    let binURL: URL
    let signature: String        // 128 hex digits
    var id: String { "\(version.text)/\(variant)" }

    /// Release asset names: JellyFloatOS-v1.2.0.bin (default) or JellyFloatOS-v1.2.0-<variant>.bin.
    static func parse(fileName: String) -> (version: String, variant: String)? {
        guard fileName.hasPrefix("JellyFloatOS-"), fileName.hasSuffix(".bin") else { return nil }
        var rest = String(fileName.dropFirst("JellyFloatOS-".count).dropLast(".bin".count))
        if rest.hasPrefix("v") { rest.removeFirst() }
        let parts = rest.split(separator: "-", maxSplits: 1).map(String.init)
        guard let version = parts.first, FirmwareVersion(version).isKnown else { return nil }
        return (version, parts.count > 1 ? parts[1] : "default")
    }
}

/// What a jelly says about updates (GET /api/ota.json, firmware 1.2.0 and later).
struct OTAStatus: Decodable, Equatable {
    var capable: Bool
    var running: Int
    var target: Int
    var state: String            // idle, receiving, verifying, failed, rebooting
    var written: Int
    var size: Int
    var busy: Bool
    var error: String
    var version: String
    var variant: String
}

/// Progress of one jelly's update, for the Jellies tab.
struct UpdateProgress: Equatable {
    enum Phase: Equatable { case checking, sending, verifying, restarting, done, failed(String) }
    var phase: Phase
    var fraction: Double = 0
}

/// Downloads signed firmware from the newest GitHub release (while the phone has internet)
/// and sends it to jellies over the jelly network, 4 KB at a time. A jelly writes it into its
/// spare partition and only boots it once it is complete and the signature matches; a broken
/// connection leaves it on its old firmware, and the next attempt resumes where it stopped.
@MainActor
final class FirmwareUpdates: ObservableObject {
    @Published private(set) var images: [FirmwareImage] = []
    @Published private(set) var downloadStatus: String?
    @Published private(set) var downloading = false
    @Published private(set) var jellyInfo: [String: OTAStatus] = [:]   // by jelly id
    @Published private(set) var notCapable: Set<String> = []          // answered, but cannot update over the air
    @Published private(set) var progress: [String: UpdateProgress] = [:]

    static let releasesAPI = URL(string: "https://api.github.com/repos/MacBachi/Jellyfish/releases/latest")!
    private let chunkSize = 4096

    private var storeDir: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        return base.appendingPathComponent("Firmware", isDirectory: true)
    }
    /// Images copied into the app's Documents folder (Finder file sharing) count as well.
    private var documentsDir: URL { FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0] }

    private let session: URLSession = {
        let c = URLSessionConfiguration.ephemeral
        c.timeoutIntervalForRequest = 6
        c.waitsForConnectivity = false
        return URLSession(configuration: c)
    }()

    init() { reload() }

    // MARK: images

    func reload() {
        var found: [FirmwareImage] = []
        let fm = FileManager.default
        for dir in [storeDir, documentsDir] {
            guard let files = try? fm.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil) else { continue }
            for url in files where url.pathExtension == "bin" {
                guard let (version, variant) = FirmwareImage.parse(fileName: url.lastPathComponent),
                      let sig = try? String(contentsOf: url.appendingPathExtension("sig"), encoding: .utf8)
                        .trimmingCharacters(in: .whitespacesAndNewlines),
                      sig.count == 128 else { continue }
                found.append(FirmwareImage(version: FirmwareVersion(version), variant: variant, binURL: url, signature: sig))
            }
        }
        images = found.sorted { $0.version > $1.version }
    }

    /// The newest image for a variant, if the app has one.
    func image(for variant: String) -> FirmwareImage? { images.first { $0.variant == variant } }

    var newestVersion: FirmwareVersion? { images.first?.version }

    /// Fetch the newest release's signed images. Needs internet, so not on the jelly network.
    func downloadLatest() async {
        guard !downloading else { return }
        downloading = true
        downloadStatus = String(localized: "Asking GitHub…")
        defer { downloading = false }
        do {
            struct Release: Decodable {
                struct Asset: Decodable { let name: String; let browser_download_url: URL }
                let tag_name: String
                let assets: [Asset]
            }
            var req = URLRequest(url: Self.releasesAPI)
            req.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
            req.timeoutInterval = 20
            let (data, _) = try await URLSession.shared.data(for: req)
            let release = try JSONDecoder().decode(Release.self, from: data)
            let bins = release.assets.filter { $0.name.hasSuffix(".bin") }
            let sigs = Dictionary(uniqueKeysWithValues: release.assets.filter { $0.name.hasSuffix(".bin.sig") }.map { ($0.name, $0) })
            let signed = bins.filter { sigs[$0.name + ".sig"] != nil }
            guard !signed.isEmpty else {
                downloadStatus = bins.isEmpty
                    ? String(localized: "Release \(release.tag_name) has no firmware for updates over the air.")
                    : String(localized: "Release \(release.tag_name) is not signed yet.")
                return
            }
            try FileManager.default.createDirectory(at: storeDir, withIntermediateDirectories: true)
            for (i, asset) in signed.enumerated() {
                downloadStatus = String(localized: "Downloading \(release.tag_name): \(i + 1) of \(signed.count)")
                let (bin, _) = try await URLSession.shared.data(from: asset.browser_download_url)
                let (sig, _) = try await URLSession.shared.data(from: sigs[asset.name + ".sig"]!.browser_download_url)
                try bin.write(to: storeDir.appendingPathComponent(asset.name), options: .atomic)
                try sig.write(to: storeDir.appendingPathComponent(asset.name + ".sig"), options: .atomic)
            }
            reload()
            downloadStatus = String(localized: "Firmware \(release.tag_name) is on this phone.")
        } catch {
            downloadStatus = String(localized: "Download failed. Is the phone on the internet?")
        }
    }

    // MARK: jellies

    /// Ask a jelly whether and how it can be updated. Firmware before 1.2.0 has no answer.
    func check(_ jelly: RosterEntry) async {
        guard !jelly.ip.isEmpty, jelly.role != .app else { return }
        if let s = try? await status(of: jelly.ip) {
            jellyInfo[jelly.id] = s
            if s.capable { notCapable.remove(jelly.id) } else { notCapable.insert(jelly.id) }
        } else {
            jellyInfo[jelly.id] = nil
            notCapable.insert(jelly.id)
        }
    }

    func update(_ jelly: RosterEntry, with image: FirmwareImage) async {
        let ip = jelly.ip
        progress[jelly.id] = UpdateProgress(phase: .checking)
        do {
            let data = try Data(contentsOf: image.binURL)
            var s = try await status(of: ip)
            guard s.capable else { throw UpdateError.message(String(localized: "This jelly needs one update over USB first.")) }
            guard s.variant == image.variant else {
                throw UpdateError.message(String(localized: "This jelly is a \(s.variant) build; the image is \(image.variant)."))
            }
            // Carry on where an interrupted transfer stopped, or start afresh.
            var offset = 0
            if s.state == "receiving" && s.size == data.count && s.written > 0 {
                offset = s.written
            } else {
                s = try await post(ip, "/api/ota/begin", body: Data("\(data.count) \(image.signature)".utf8))
                guard s.state == "receiving" else { throw UpdateError.message(s.error.isEmpty ? String(localized: "The jelly refused the update.") : s.error) }
            }
            var failures = 0
            while offset < data.count {
                let end = min(offset + chunkSize, data.count)
                var body = Data(count: 4)
                body.withUnsafeMutableBytes { $0.storeBytes(of: UInt32(offset).littleEndian, as: UInt32.self) }
                body.append(data[offset..<end])
                progress[jelly.id] = UpdateProgress(phase: .sending, fraction: Double(offset) / Double(data.count))
                do {
                    _ = try await post(ip, "/api/ota/chunk", body: body)
                    s = try await waitUntil(ip) { $0.written >= end || $0.state == "failed" }
                    if s.state == "failed" { throw UpdateError.message(s.error) }
                    offset = s.written
                    failures = 0
                } catch UpdateError.message(let m) {
                    throw UpdateError.message(m)
                } catch {
                    // A dropped packet or a busy jelly: ask where it stands and go on from there.
                    failures += 1
                    if failures > 8 { throw UpdateError.message(String(localized: "Lost the connection to the jelly. Try again; it resumes.")) }
                    try? await Task.sleep(nanoseconds: 500_000_000)
                    if let now = try? await status(of: ip), now.state == "receiving" { offset = now.written }
                }
            }
            progress[jelly.id] = UpdateProgress(phase: .verifying, fraction: 1)
            _ = try await post(ip, "/api/ota/finish", body: Data())
            s = try await waitUntil(ip, timeout: 15) { $0.state == "rebooting" || $0.state == "failed" }
            if s.state == "failed" { throw UpdateError.message(s.error) }
            progress[jelly.id] = UpdateProgress(phase: .restarting, fraction: 1)
            try? await Task.sleep(nanoseconds: 12_000_000_000)
            progress[jelly.id] = UpdateProgress(phase: .done, fraction: 1)
        } catch UpdateError.message(let m) {
            progress[jelly.id] = UpdateProgress(phase: .failed(m))
        } catch {
            progress[jelly.id] = UpdateProgress(phase: .failed(String(localized: "The jelly did not answer.")))
        }
    }

    // MARK: HTTP

    private enum UpdateError: Error { case message(String) }

    private func status(of ip: String) async throws -> OTAStatus {
        let (data, response) = try await session.data(from: URL(string: "http://\(ip)/api/ota.json")!)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else { throw URLError(.badServerResponse) }
        return try JSONDecoder().decode(OTAStatus.self, from: data)
    }

    private func post(_ ip: String, _ path: String, body: Data) async throws -> OTAStatus {
        var req = URLRequest(url: URL(string: "http://\(ip)\(path)")!)
        req.httpMethod = "POST"
        req.setValue("application/octet-stream", forHTTPHeaderField: "Content-Type")
        req.httpBody = body
        let (data, _) = try await session.data(for: req)
        return try JSONDecoder().decode(OTAStatus.self, from: data)
    }

    /// Poll the status until the jelly has caught up (it writes each chunk from its main loop).
    private func waitUntil(_ ip: String, timeout: Double = 5, _ done: (OTAStatus) -> Bool) async throws -> OTAStatus {
        let deadline = Date().addingTimeInterval(timeout)
        var s = try await status(of: ip)
        while !done(s) {
            if Date() > deadline { throw URLError(.timedOut) }
            try await Task.sleep(nanoseconds: 40_000_000)
            s = try await status(of: ip)
        }
        return s
    }
}
