import Combine
import Foundation
import SwiftUI

/// The one place the interface talks to: settings, the connection, what the bloom is doing,
/// who is in it, and the virtual jelly's engine.
@MainActor
final class AppModel: ObservableObject {
    enum Connection: Equatable {
        case idle, joiningWiFi, connecting, connected, demo, failed(String)

        var label: String {
            switch self {
            case .idle: return "Not connected"
            case .joiningWiFi: return "Joining the jelly network"
            case .connecting: return "Looking for the bloom"
            case .connected: return "In the bloom"
            case .demo: return "Demo bloom"
            case .failed(let why): return why
            }
        }
        var isLive: Bool { self == .connected || self == .demo }
    }

    let settings = AppSettings()
    @Published private(set) var connection: Connection = .idle
    @Published private(set) var state = JellyState()
    @Published private(set) var slot = -1
    @Published private(set) var apID: String?
    @Published private(set) var roster: [RosterEntry] = []
    @Published private(set) var level = 0.0
    @Published private(set) var beatCount = 0
    @Published private(set) var lastBeatAt: Date?
    @Published private(set) var identStartUs: Int64 = 0
    @Published private(set) var lastHeardAt: Date?
    @Published private(set) var linkStatus = ""
    @Published var showJoinPrompt = false
    @Published var joinError: String?

    @Published private(set) var engine: JellyEngine
    private var timeSync = TimeSync()
    private var transport: JellyTransport?
    private var helloTask: Task<Void, Never>?
    private var pendingBeat = false
    private var seenBeats = 0

    init() {
        engine = JellyEngine(layout: JellyLayout(ringLEDs: settings.virtualRingLEDs))
    }

    // MARK: lifecycle

    func scenePhaseChanged(_ phase: ScenePhase) {
        switch phase {
        case .active:
            if connection.isLive { return }
            if settings.demoMode { startDemo(); return }
            if settings.joinOnForeground {
                if settings.askBeforeJoining { showJoinPrompt = true } else { Task { await joinAndConnect() } }
            }
        case .background:
            stop()
        default: break
        }
    }

    /// Join the configured Wi-Fi (system dialog), then start talking to the AP jelly.
    func joinAndConnect() async {
        showJoinPrompt = false
        joinError = nil
        connection = .joiningWiFi
        let outcome = await NetworkJoiner.join(ssid: settings.ssid, password: settings.password, joinOnce: !settings.stayConnectedInBackground)
        switch outcome {
        case .joined, .alreadyConnected:
            startNetwork()
        case .denied:
            connection = .idle
        case .unavailable(let why), .failed(let why):
            joinError = why
            connection = .failed(why)
        }
    }

    /// Skip the Wi-Fi step (already on the network, or joined by hand) and just connect.
    func connectWithoutJoining() {
        showJoinPrompt = false
        startNetwork()
    }

    func startDemo() {
        stop()
        connection = .demo
        let demo = DemoJelly()
        attach(demo)
    }

    private func startNetwork() {
        stop()
        connection = .connecting
        attach(UDPJellyLink())
    }

    private func attach(_ t: JellyTransport) {
        transport = t
        t.onLine = { [weak self] line, at in self?.handle(line, receivedAtUs: at) }
        t.onStatus = { [weak self] s in self?.linkStatus = s }
        t.connect()
        // Announce ourselves now and every 5 s: that is what keeps the AP's unicast copies coming.
        helloTask?.cancel()
        helloTask = Task { [weak self] in
            while !Task.isCancelled {
                self?.sendHello()
                try? await Task.sleep(for: .seconds(5))
            }
        }
    }

    func stop() {
        helloTask?.cancel(); helloTask = nil
        transport?.disconnect(); transport = nil
        if connection.isLive || connection == .connecting { connection = .idle }
        timeSync.reset()
        slot = -1
        roster = []
    }

    // MARK: commands

    func select(_ mode: JellyMode) { state.mode = mode; send(OutboundLine.mode(mode)) }
    func stepMode(_ delta: Int) {
        let n = JellyMode.allCases.count
        let next = JellyMode(rawValue: ((state.mode.rawValue + delta) % n + n) % n)!
        select(next)
    }
    func setBrightness(_ v: Double) { state.brightness = v; throttled("BRIGHT") { self.send(OutboundLine.brightness(v)) } }
    func setHue(_ v: Double) { state.hueOffset = v; throttled("HUE") { self.send(OutboundLine.hue(v)) } }
    func setCycle(_ v: Double) { state.cyclePeriod = v; throttled("CYCLE") { self.send(OutboundLine.cycle(v)) } }
    func identify() { send(OutboundLine.identify) }
    func rollCall() { send(OutboundLine.rollCall) }
    func sendBeat() { send(OutboundLine.beat) }

    private var throttleTasks: [String: Task<Void, Never>] = [:]
    /// Sliders fire constantly; send at most every 60 ms per parameter, always the latest value.
    private func throttled(_ key: String, _ action: @escaping () -> Void) {
        guard throttleTasks[key] == nil else { pendingActions[key] = action; return }
        action()
        throttleTasks[key] = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(60))
            guard let self else { return }
            self.throttleTasks[key] = nil
            if let later = self.pendingActions.removeValue(forKey: key) { self.throttled(key, later) }
        }
    }
    private var pendingActions: [String: () -> Void] = [:]

    private func send(_ line: String) { transport?.send(line) }

    private func sendHello() { send(OutboundLine.hello(id: settings.jellyID, slot: slot)) }

    // MARK: inbound

    private func handle(_ raw: String, receivedAtUs: Int64) {
        lastHeardAt = Date()
        if connection == .connecting { connection = .connected }
        switch InboundLine.parse(raw) {
        case .state(let s, let apTimeUs, let id):
            state = s
            apID = id
            timeSync.update(apTimeUs: apTimeUs, receivedAtUs: receivedAtUs)
            touchRoster(RosterEntry(id: id, role: .ap, slot: 0, ip: "192.168.4.1", lastSeen: Date()))
        case .hello(let entry):
            touchRoster(entry)
        case .slot(let id, let n):
            if id == settings.jellyID { slot = n }
        case .beat:
            beatCount += 1; lastBeatAt = Date(); pendingBeat = true
        case .ident(let startUs):
            identStartUs = startUs
            Task { try? await Task.sleep(for: .seconds(2.5)); if self.identStartUs == startUs { self.identStartUs = 0 } }
        case .level(let l):
            level = l
        case .mode(let m):
            if let mode = JellyMode(rawValue: m) { state.mode = mode }
        case .brightness(let v): state.brightness = v
        case .hue(let v): state.hueOffset = v
        case .cycle(let v): state.cyclePeriod = v
        case .unknown: break
        }
    }

    private func touchRoster(_ entry: RosterEntry) {
        // The AP replays our own HELLO back to us; that carries our slot, nothing more.
        if entry.role == .app && entry.id == settings.jellyID { slot = entry.slot; return }
        var e = entry
        if let i = roster.firstIndex(where: { $0.id == entry.id }) {
            // A STATE line names the AP without version or address: keep what a HELLO told us.
            if e.firmware == nil { e.firmware = roster[i].firmware; e.modeCount = roster[i].modeCount }
            if e.ip.isEmpty { e.ip = roster[i].ip }
            roster[i] = e
        } else {
            roster.append(e)
        }
        roster.sort { ($0.role == .ap ? -1 : $0.slot) < ($1.role == .ap ? -1 : $1.slot) }
    }

    // MARK: firmware overview

    /// The jellies proper, without other phones.
    var jellies: [RosterEntry] { roster.filter { $0.role != .app } }

    /// The newest firmware anyone in the bloom announced, if anyone did.
    var newestFirmwareInBloom: FirmwareVersion? { jellies.compactMap(\.firmwareVersion).max() }

    /// Jellies that said how many modes they have and lack this one. They still take the
    /// command; the firmware wraps the number around, see `RosterEntry.fallback(for:)`.
    func jelliesLacking(_ mode: JellyMode) -> [RosterEntry] { jellies.filter { $0.knows(mode) == false } }

    /// One line on the state of the bloom's firmware, for the Jellies tab.
    var firmwareSummary: String {
        let js = jellies
        if js.isEmpty { return connection.isLive ? "No jelly has introduced itself yet." : "Connect to see what the jellies run." }
        var groups: [(String, Int)] = []
        for j in js {
            let key = j.firmwareVersion?.text ?? "no version"
            if let i = groups.firstIndex(where: { $0.0 == key }) { groups[i].1 += 1 } else { groups.append((key, 1)) }
        }
        let latest = BuildInfo.latestFirmware
        if groups.count == 1, let only = groups.first {
            if only.0 == latest.text { return js.count == 1 ? "The jelly runs \(only.0), the same as this app." : "All \(js.count) jellies run \(only.0), the same as this app." }
            if only.0 == "no version" { return "\(js.count == 1 ? "The jelly reports" : "The jellies report") no version: firmware from before \(latest.text). Modes added since may not exist there." }
        }
        let parts = groups.map { "\($0.1) × \($0.0)" }.joined(separator: ", ")
        var text = "Mixed firmware: \(parts)."
        if js.contains(where: { $0.firmwareStatus == .newer }) { text += " One jelly is newer than this app; it may have modes the app cannot show." }
        if js.contains(where: { $0.firmwareStatus == .older || $0.firmwareStatus == .unknown }) { text += " Newer modes fall back to another mode on older jellies; nothing is blocked." }
        return text
    }

    // MARK: virtual jelly

    var masterNowUs: Int64 { timeSync.masterNowUs() }

    /// One frame of the virtual jelly. Call from the drawing timeline.
    func renderFrame() -> JellyFrame {
        let beat = pendingBeat; pendingBeat = false
        let input = JellyEngine.Input(
            mode: state.mode, masterUs: masterNowUs, slot: slot, cyclePeriod: state.cyclePeriod,
            brightness: state.brightness, hueOffset: state.hueOffset, level: level, beat: beat,
            identStartUs: identStartUs, isAP: false)
        return engine.render(input)
    }

    var effectiveMode: JellyMode { JellyEngine.effectiveMode(state.mode, masterUs: masterNowUs) }

    func rebuildEngine() { engine = JellyEngine(layout: JellyLayout(ringLEDs: settings.virtualRingLEDs)) }
}
