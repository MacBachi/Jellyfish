import Foundation

/// A pretend access point jelly living inside the app, so the interface can be used in the
/// simulator and without hardware. Speaks the same lines the firmware does.
final class DemoJelly: JellyTransport {
    var onLine: ((String, Int64) -> Void)?
    var onStatus: ((String) -> Void)?

    private var state = JellyState()
    private var timer: Timer?
    private var tick = 0
    private var level = 0.1
    private var slotsByID: [String: Int] = [:]
    private let apID = "0451"
    private let startUs = TimeSync.nowUs()
    private var nextBeatUs: Int64 = 0
    private var beatCount = 0
    private var trims: [String: Double] = ["0451": 1, "1b3c": 1.25, "c7d1": 0.8]

    func connect() {
        onStatus?("demo")
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in self?.step() }
        emit(apHello)
    }

    private var apHello: String { "HELLO \(apID) AP 0 192.168.4.1 \(BuildInfo.appVersion) \(BuildInfo.modeCount) \(mic("0451"))" }
    /// A pretend microphone calibration: level, floor, ceiling, trim (1.2.0 hellos).
    private func mic(_ id: String) -> String { String(format: "%.2f %.0f %.0f %.2f", level, 61000.0, 140000.0, trims[id] ?? 1) }
    /// Three pretend stations: one current, one on an older firmware that knows fewer modes, and
    /// one from before version reporting, so the Jellies tab has something to show.
    private var stationHellos: [String] {
        ["HELLO 1b3c STA 2 192.168.4.17 \(BuildInfo.appVersion) \(BuildInfo.modeCount) \(mic("1b3c"))",
         "HELLO c7d1 STA 3 192.168.4.31 0.1.0 \(BuildInfo.modeCount - 1)",
         "HELLO 9e02 STA 4 192.168.4.22"]
    }

    func disconnect() { timer?.invalidate(); timer = nil }

    private var apTimeUs: Int64 { TimeSync.nowUs() - startUs }

    private func emit(_ line: String) { onLine?(line, TimeSync.nowUs()) }

    private func emitState() {
        emit(String(format: "STATE %d %.2f %.0f %.1f %lld %@ %d %d %.2f", state.mode.rawValue, state.brightness, state.hueOffset, state.cyclePeriod, apTimeUs, apID, state.noodleFollow ? 1 : 0, beatCount, state.sensitivity))
        // the tempo line that follows every STATE: a steady 120 BPM with a beat at the start
        emit(String(format: "TEMPO %d %lld %.2f", 500_000, (apTimeUs / 500_000) * 500_000, 0.8))
    }

    private func step() {
        tick += 1
        // a wandering "room" level
        level += Double.random(in: -0.06...0.06)
        level = min(max(level, 0.02), 0.9)
        emit(String(format: "LEVEL %.2f", level))
        if tick % 10 == 0 { emitState() }
        if state.mode == .drops || state.mode == .relay, apTimeUs >= nextBeatUs {
            beatCount += 1
            emit("BEAT \(beatCount)")
            nextBeatUs = apTimeUs + Int64.random(in: 400_000...900_000)
        }
    }

    func send(_ line: String) {
        let parts = line.split(separator: " ").map(String.init)
        guard let verb = parts.first?.uppercased() else { return }
        func arg(_ i: Int) -> String? { i < parts.count ? parts[i] : nil }
        switch verb {
        case "MODE":
            if let m = arg(1).flatMap(Int.init).flatMap(JellyMode.init(rawValue:)) { state.mode = m; emitState() }
        case "NEXT": state.mode = JellyMode(rawValue: (state.mode.rawValue + 1) % JellyMode.allCases.count)!; emit("MODE \(state.mode.rawValue)"); emitState()
        case "PREV": state.mode = JellyMode(rawValue: (state.mode.rawValue + JellyMode.allCases.count - 1) % JellyMode.allCases.count)!; emit("MODE \(state.mode.rawValue)"); emitState()
        case "BRIGHT": if let v = arg(1).flatMap(Double.init) { state.brightness = min(max(v, 0), 1); emitState() }
        case "HUE": if let v = arg(1).flatMap(Double.init) { state.hueOffset = v.truncatingRemainder(dividingBy: 360); emitState() }
        case "CYCLE": if let v = arg(1).flatMap(Double.init) { state.cyclePeriod = max(v, 1); emitState() }
        case "NOODLE": if let v = arg(1).flatMap(Int.init) { state.noodleFollow = v != 0; emitState() }
        case "BEAT": beatCount += 1; emit("BEAT \(beatCount)")
        case "SENS": if let v = arg(1).flatMap(Double.init) { state.sensitivity = min(max(v, 0.1), 3); emitState() }
        case "TRIM": if let id = arg(1), let v = arg(2).flatMap(Double.init) { trims[id] = min(max(v, 0.25), 4) }
        case "IDENT": emit("IDENT \(apTimeUs + 200_000)")
        case "BLINK": if let id = arg(1) { emit("IDENT \(apTimeUs + 200_000) \(id)") }
        case "HELLO":
            if parts.count >= 3, let id = arg(1) {
                let slot = slotsByID[id] ?? (slotsByID.count + 5)
                slotsByID[id] = slot
                emit("SLOT \(id) \(slot)")
                // like the firmware: answer an app's hello with the whole roster
                emit(apHello)
                stationHellos.forEach { emit($0) }
                emit("HELLO \(id) APP \(slot) app \(parts.count > 5 ? parts[5] : "?") \(parts.count > 6 ? parts[6] : "0")")
            } else {
                // roll call: the AP and the pretend stations answer, a little apart
                emit(apHello)
                for (i, line) in stationHellos.enumerated() {
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.12 + 0.07 * Double(i)) { [weak self] in self?.emit(line) }
                }
            }
        default: break
        }
    }
}
