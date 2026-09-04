import Foundation

/// What this build is. The values come from Info.plist, which scripts/gen-build-info.sh fills
/// from the repository's VERSION file and git at build time; firmware built from the same
/// repository state carries the same version.
enum BuildInfo {
    private static let info = Bundle.main.infoDictionary ?? [:]
    static let appVersion = info["JellyVersion"] as? String ?? info["CFBundleShortVersionString"] as? String ?? "0.0.0"
    static let build = info["CFBundleVersion"] as? String ?? "0"
    static let gitRevision = info["JellyGitRevision"] as? String ?? "unknown"

    /// The newest firmware this app knows of: the one from its own repository state.
    static let latestFirmware = FirmwareVersion(appVersion)

    /// How many modes this app, and firmware of the same version, know.
    static let modeCount = JellyMode.allCases.count
}

/// A version such as "0.2.0" or "0.2.0-dev", ordered numerically. Jellies running firmware
/// from before version reporting send "?", which parses as unknown.
struct FirmwareVersion: Equatable, Comparable, CustomStringConvertible {
    let text: String
    let numbers: [Int]
    let prerelease: String?

    init(_ text: String) {
        self.text = text
        let core = text.split(separator: "-", maxSplits: 1).map(String.init)
        let parts = core.first.map { $0.split(separator: ".").map(String.init) } ?? []
        let nums = parts.compactMap(Int.init)
        numbers = nums.count == parts.count && !nums.isEmpty ? nums : []
        prerelease = core.count > 1 ? core[1] : nil
    }

    var isKnown: Bool { !numbers.isEmpty }
    var description: String { isKnown ? text : "no version" }

    static func < (a: FirmwareVersion, b: FirmwareVersion) -> Bool {
        let n = max(a.numbers.count, b.numbers.count)
        for i in 0..<n {
            let x = i < a.numbers.count ? a.numbers[i] : 0
            let y = i < b.numbers.count ? b.numbers[i] : 0
            if x != y { return x < y }
        }
        // 0.2.0-dev comes before 0.2.0
        switch (a.prerelease, b.prerelease) {
        case (nil, nil): return false
        case (.some, nil): return true
        case (nil, .some): return false
        case (.some(let p), .some(let q)): return p < q
        }
    }
}

/// How a jelly's firmware relates to the newest one this app knows.
enum FirmwareStatus: Equatable {
    case latest, older, newer, unknown

    var label: String {
        switch self {
        case .latest: return String(localized: "up to date")
        case .older: return String(localized: "older")
        case .newer: return String(localized: "newer than this app")
        case .unknown: return String(localized: "no version")
        }
    }
}

extension RosterEntry {
    var firmwareVersion: FirmwareVersion? { firmware.map(FirmwareVersion.init).flatMap { $0.isKnown ? $0 : nil } }

    var firmwareStatus: FirmwareStatus {
        guard let v = firmwareVersion else { return .unknown }
        if v == BuildInfo.latestFirmware { return .latest }
        return v < BuildInfo.latestFirmware ? .older : .newer
    }

    /// Whether this jelly knows a mode; nil when it did not say how many modes it has.
    func knows(_ mode: JellyMode) -> Bool? {
        guard let n = modeCount, n > 0 else { return nil }
        return mode.rawValue < n
    }

    /// What this jelly shows when told a mode it lacks: the firmware wraps the number around.
    func fallback(for mode: JellyMode) -> JellyMode? {
        guard let n = modeCount, n > 0, mode.rawValue >= n else { return nil }
        return JellyMode(rawValue: mode.rawValue % n)
    }

    /// Modes this app has that the jelly lacks.
    var missingModes: [JellyMode] { JellyMode.allCases.filter { knows($0) == false } }
}
