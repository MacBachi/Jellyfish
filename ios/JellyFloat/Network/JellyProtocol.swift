import Foundation

/// The display modes, in the firmware's order. The raw value is what `MODE n` sends.
enum JellyMode: Int, CaseIterable, Identifiable, Codable {
    case breathe = 0, glimmer, aurora, current, lantern, moonlight, drizzle, fireflies, swarm, whisper
    case playlist
    case micField, drops
    case palette, paletteCycle
    case ambientRainbow, ambientDeepSea
    case micLevelCheck, ledChannelTest
    case sos

    var id: Int { rawValue }

    enum Group: String, CaseIterable, Identifiable {
        case calm = "Calm", sound = "Sound", colour = "Colour", ambient = "Ambient", signal = "Signal", test = "Test"
        var id: String { rawValue }
    }

    var group: Group {
        switch self {
        case .breathe, .glimmer, .aurora, .current, .lantern, .moonlight, .drizzle, .fireflies, .swarm, .whisper, .playlist: return .calm
        case .micField, .drops: return .sound
        case .palette, .paletteCycle: return .colour
        case .ambientRainbow, .ambientDeepSea: return .ambient
        case .micLevelCheck, .ledChannelTest: return .test
        case .sos: return .signal
        }
    }

    var name: String {
        switch self {
        case .breathe: return "Breathe"
        case .glimmer: return "Glimmer"
        case .aurora: return "Aurora"
        case .current: return "Current"
        case .lantern: return "Lantern"
        case .moonlight: return "Moonlight"
        case .drizzle: return "Drizzle"
        case .fireflies: return "Fireflies"
        case .swarm: return "Swarm"
        case .whisper: return "Whisper"
        case .playlist: return "Playlist"
        case .micField: return "Sound field"
        case .drops: return "Drops"
        case .palette: return "Palette"
        case .paletteCycle: return "Palette cycle"
        case .ambientRainbow: return "Rainbow"
        case .ambientDeepSea: return "Deep sea"
        case .micLevelCheck: return "Mic level"
        case .ledChannelTest: return "Channel test"
        case .sos: return "SOS"
        }
    }

    var blurb: String {
        switch self {
        case .breathe: return "A slow pulse from the bell down the tentacles"
        case .glimmer: return "Near dark, with sparks that glow and fade"
        case .aurora: return "Bands of green, teal and violet drifting by"
        case .current: return "A gentle wave travelling up the tentacles"
        case .lantern: return "Warm amber with a hint of candle flicker"
        case .moonlight: return "Very dim, cool, hardly moving"
        case .drizzle: return "Single slow drops with trails"
        case .fireflies: return "Lone lights rising and falling"
        case .swarm: return "A pulse visiting one jelly after the other"
        case .whisper: return "Follows the room's sound, slowly"
        case .playlist: return "Wanders through the calm modes, all jellies together"
        case .micField: return "The original sound-reactive field"
        case .drops: return "Drops on every beat"
        case .palette: return "One colour per jelly"
        case .paletteCycle: return "Colours rotate through the bloom"
        case .ambientRainbow: return "Every colour, slowly"
        case .ambientDeepSea: return "Blues and greens, slowly"
        case .micLevelCheck: return "Prints levels to the serial console"
        case .ledChannelTest: return "Red, green, blue, one noodle at a time"
        case .sos: return "Red Morse code, all jellies in step"
        }
    }

    /// Representative hues (degrees) for the tile artwork.
    var tintHues: [Double] {
        switch self {
        case .breathe: return [200, 215]
        case .glimmer: return [175, 190]
        case .aurora: return [130, 190, 260]
        case .current: return [185, 200]
        case .lantern: return [32, 18]
        case .moonlight: return [215, 220]
        case .drizzle: return [205, 195]
        case .fireflies: return [78, 60]
        case .swarm: return [0, 120, 220]
        case .whisper: return [200, 120, 40]
        case .playlist: return [200, 40, 300]
        case .micField: return [220, 300]
        case .drops: return [220, 190]
        case .palette: return [0, 60, 180, 270]
        case .paletteCycle: return [30, 120, 220, 310]
        case .ambientRainbow: return [0, 90, 180, 270]
        case .ambientDeepSea: return [170, 220, 250]
        case .micLevelCheck: return [220]
        case .ledChannelTest: return [0, 120, 240]
        case .sos: return [0, 350]
        }
    }
}

/// The firmware's palette, hue in degrees, by colour slot.
enum JellyPalette {
    static let hues: [Double] = [0, 30, 60, 120, 180, 220, 270, 310]
    static func hue(forSlot slot: Int) -> Double { hues[max(slot, 0) % hues.count] }
}

/// What the AP broadcasts in every STATE line.
struct JellyState: Equatable {
    var mode: JellyMode = .micField
    var brightness: Double = 1
    var hueOffset: Double = 0
    var cyclePeriod: Double = 10
}

struct RosterEntry: Identifiable, Equatable {
    enum Role: String { case ap = "AP", station = "STA", app = "APP" }
    var id: String
    var role: Role
    var slot: Int
    var ip: String
    var lastSeen: Date
    /// Firmware version as announced in HELLO; nil from firmware that predates version reporting.
    var firmware: String? = nil
    /// How many modes the jelly knows (mode numbers 0..<modeCount); nil when it did not say.
    var modeCount: Int? = nil
}

/// One line received from the network, already parsed.
enum InboundLine: Equatable {
    case state(JellyState, apTimeUs: Int64, apID: String)
    case hello(RosterEntry)
    case slot(id: String, slot: Int)
    case beat
    case ident(startUs: Int64)
    case level(Double)
    case mode(Int)
    case brightness(Double)
    case hue(Double)
    case cycle(Double)
    case unknown(String)

    static func parse(_ raw: String, at date: Date = Date()) -> InboundLine {
        let parts = raw.trimmingCharacters(in: .whitespacesAndNewlines).split(separator: " ").map(String.init)
        guard let verb = parts.first?.uppercased() else { return .unknown(raw) }
        let args = Array(parts.dropFirst())
        func d(_ i: Int) -> Double? { i < args.count ? Double(args[i]) : nil }
        func n(_ i: Int) -> Int? { i < args.count ? Int(args[i]) : nil }
        switch verb {
        case "STATE":
            guard let m = n(0), let b = d(1), let h = d(2), let c = d(3), let t = args.count > 4 ? Int64(args[4]) : nil, args.count > 5 else { return .unknown(raw) }
            let mode = JellyMode(rawValue: m) ?? .micField
            return .state(JellyState(mode: mode, brightness: b, hueOffset: h, cyclePeriod: c), apTimeUs: t, apID: args[5])
        case "HELLO":
            // HELLO <id> <role> <slot> [<ip> [<version> <modes>]]; "?" and 0 stand for "not known".
            guard args.count >= 3, let role = RosterEntry.Role(rawValue: args[1].uppercased()), let s = n(2) else { return .unknown(raw) }
            let ip = args.count > 3 && args[3] != "?" ? args[3] : ""
            let firmware = args.count > 4 && args[4] != "?" ? args[4] : nil
            let modes = n(5).flatMap { $0 > 0 ? $0 : nil }
            return .hello(RosterEntry(id: args[0], role: role, slot: s, ip: ip, lastSeen: date, firmware: firmware, modeCount: modes))
        case "SLOT":
            guard args.count >= 2, let s = n(1) else { return .unknown(raw) }
            return .slot(id: args[0], slot: s)
        case "BEAT": return .beat
        case "IDENT":
            guard let t = args.first.flatMap({ Int64($0) }) else { return .unknown(raw) }
            return .ident(startUs: t)
        case "LEVEL": return d(0).map { .level($0) } ?? .unknown(raw)
        case "MODE": return n(0).map { .mode($0) } ?? .unknown(raw)
        case "BRIGHT": return d(0).map { .brightness($0) } ?? .unknown(raw)
        case "HUE": return d(0).map { .hue($0) } ?? .unknown(raw)
        case "CYCLE": return d(0).map { .cycle($0) } ?? .unknown(raw)
        default: return .unknown(raw)
        }
    }
}

/// Lines the app sends. One per datagram, no newline needed.
enum OutboundLine {
    static func mode(_ m: JellyMode) -> String { "MODE \(m.rawValue)" }
    static let next = "NEXT"
    static let prev = "PREV"
    static func brightness(_ v: Double) -> String { String(format: "BRIGHT %.2f", v) }
    static func hue(_ v: Double) -> String { String(format: "HUE %.0f", v) }
    static func cycle(_ v: Double) -> String { String(format: "CYCLE %.1f", v) }
    static let identify = "IDENT"
    static let rollCall = "HELLO"
    static let beat = "BEAT"
    static func hello(id: String, slot: Int) -> String { "HELLO \(id) APP \(slot) app \(BuildInfo.appVersion) \(BuildInfo.modeCount)" }
}
