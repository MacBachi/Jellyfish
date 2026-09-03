import Foundation
import Observation
import SwiftUI

/// The one place the interface talks to: settings, the connection, what the bloom is doing,
/// who is in it, and the virtual jelly's engine.
@MainActor
@Observable
final class AppModel {
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
    private(set) var connection: Connection = .idle
    private(set) var state = JellyState()
    private(set) var slot = -1
    private(set) var apID: String?
    private(set) var roster: [RosterEntry] = []
    private(set) var level = 0.0
    private(set) var beatCount = 0
    private(set) var lastBeatAt: Date?
    private(set) var identStartUs: Int64 = 0
    private(set) var lastHeardAt: Date?
    private(set) var linkStatus = ""
    var showJoinPrompt = false
    var joinError: String?

    private(set) var engine: JellyEngine
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
        if let i = roster.firstIndex(where: { $0.id == entry.id }) { roster[i] = entry } else { roster.append(entry) }
        roster.sort { ($0.role == .ap ? -1 : $0.slot) < ($1.role == .ap ? -1 : $1.slot) }
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
