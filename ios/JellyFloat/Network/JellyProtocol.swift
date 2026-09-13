import Foundation

/// The display modes, in the firmware's order. The raw value is what `MODE n` sends.
enum JellyMode: Int, CaseIterable, Identifiable, Codable {
    case breathe = 0, glimmer, aurora, current, lantern, moonlight, drizzle, fireflies, swarm, whisper
    case playlist
    case micField, drops, sundown, tide, pulse, rise, voice, relay
    case palette, paletteCycle
    case ambientRainbow, ambientDeepSea
    case micLevelCheck, ledChannelTest
    case sos

    var id: Int { rawValue }

    enum Group: String, CaseIterable, Identifiable {
        case calm = "Calm", sound = "Sound", colour = "Colour", ambient = "Ambient", signal = "Signal", test = "Test"
        var id: String { rawValue }
        /// The group's name in the user's language; the raw value is the English key.
        var title: String { String(localized: String.LocalizationValue(rawValue)) }
    }

    var group: Group {
        switch self {
        case .breathe, .glimmer, .aurora, .current, .lantern, .moonlight, .drizzle, .fireflies, .swarm, .whisper, .playlist: return .calm
        case .micField, .drops, .sundown, .tide, .pulse, .rise, .voice, .relay: return .sound
        case .palette, .paletteCycle: return .colour
        case .ambientRainbow, .ambientDeepSea: return .ambient
        case .micLevelCheck, .ledChannelTest: return .test
        case .sos: return .signal
        }
    }

    var name: String {
        switch self {
        case .breathe: return String(localized: "Breathe")
        case .glimmer: return String(localized: "Glimmer")
        case .aurora: return String(localized: "Aurora")
        case .current: return String(localized: "Current")
        case .lantern: return String(localized: "Lantern")
        case .moonlight: return String(localized: "Moonlight")
        case .drizzle: return String(localized: "Drizzle")
        case .fireflies: return String(localized: "Fireflies")
        case .swarm: return String(localized: "Swarm")
        case .whisper: return String(localized: "Whisper")
        case .playlist: return String(localized: "Playlist")
        case .micField: return String(localized: "Sound field")
        case .drops: return String(localized: "Drops")
        case .sundown: return String(localized: "Sundown")
        case .tide: return String(localized: "Tide")
        case .pulse: return String(localized: "Pulse")
        case .rise: return String(localized: "Rise")
        case .voice: return String(localized: "Voice")
        case .relay: return String(localized: "Relay")
        case .palette: return String(localized: "Palette")
        case .paletteCycle: return String(localized: "Palette cycle")
        case .ambientRainbow: return String(localized: "Rainbow")
        case .ambientDeepSea: return String(localized: "Deep sea")
        case .micLevelCheck: return String(localized: "Mic level")
        case .ledChannelTest: return String(localized: "Channel test")
        case .sos: return String(localized: "SOS")
        }
    }

    var blurb: String {
        switch self {
        case .breathe: return String(localized: "A slow pulse from the bell down the tentacles")
        case .glimmer: return String(localized: "Near dark, with sparks that glow and fade")
        case .aurora: return String(localized: "Bands of green, teal and violet drifting by")
        case .current: return String(localized: "A gentle wave travelling up the tentacles")
        case .lantern: return String(localized: "Warm amber with a hint of candle flicker")
        case .moonlight: return String(localized: "Very dim, cool, hardly moving")
        case .drizzle: return String(localized: "Single slow drops with trails")
        case .fireflies: return String(localized: "Lone lights rising and falling")
        case .swarm: return String(localized: "A pulse visiting one jelly after the other")
        case .whisper: return String(localized: "Follows the room's sound, slowly")
        case .playlist: return String(localized: "Wanders through the calm modes, all jellies together")
        case .micField: return String(localized: "The original sound-reactive field")
        case .drops: return String(localized: "Drops on every beat")
        case .sundown: return String(localized: "A sunset that swells with the bass")
        case .tide: return String(localized: "The last seconds of the bass wash down the tentacles")
        case .pulse: return String(localized: "Breathes on the beat, not on every kick")
        case .rise: return String(localized: "Walks from cool to gold as the music builds, and blooms on the drop")
        case .voice: return String(localized: "A bright spot that follows the lead line around the ring")
        case .relay: return String(localized: "Every beat hands the pulse to the next jelly")
        case .palette: return String(localized: "One colour per jelly")
        case .paletteCycle: return String(localized: "Colours rotate through the bloom")
        case .ambientRainbow: return String(localized: "Every colour, slowly")
        case .ambientDeepSea: return String(localized: "Blues and greens, slowly")
        case .micLevelCheck: return String(localized: "Prints levels to the serial console")
        case .ledChannelTest: return String(localized: "Red, green, blue, one noodle at a time")
        case .sos: return String(localized: "Red Morse code, all jellies in step")
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
        case .sundown: return [338, 30, 20]
        case .tide: return [196, 234]
        case .pulse: return [195, 330]
        case .rise: return [215, 42]
        case .voice: return [275, 55]
        case .relay: return [0, 120, 220]
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

/// The microphone as the AP hears it: the whole signal and the three bands of its filter
/// bank, each already stretched and enveloped. The sound-reactive modes follow the bands.
struct AudioLevels: Equatable {
    var level: Double = 0
    var bass: Double = 0
    var mid: Double = 0
    var melody: Double = 0      // 300 Hz to 800 Hz: the lead, the voice
    var treble: Double = 0
    var rise: Double = 0        // how far the last seconds sit above the last half minute
    var quiet: Double = 0       // 1 once the room has been silent for a few seconds
    var tempoPhase: Double = 0  // 0..1, zero on the beat
    var tempoConfidence: Double = 0
}

/// What the AP broadcasts in every STATE line.
struct JellyState: Equatable {
    var mode: JellyMode = .micField
    var brightness: Double = 0.2 // the firmware's DEFAULT_BRIGHTNESS
    var hueOffset: Double = 0
    var cyclePeriod: Double = 10
    /// Whether the brightness setting also dims the filament LEDs. The firmware's default is off.
    var noodleFollow: Bool = false
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
    case level(AudioLevels)
    case mode(Int)
    case brightness(Double)
    case hue(Double)
    case cycle(Double)
    case noodleFollow(Bool)
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
            let follow = args.count > 6 ? args[6] != "0" : true
            return .state(JellyState(mode: mode, brightness: b, hueOffset: h, cyclePeriod: c, noodleFollow: follow),
                          apTimeUs: t, apID: args[5])
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
        case "LEVEL":
            guard let l = d(0) else { return .unknown(raw) }
            // Firmware before the filter bank sends the level alone.
            return .level(AudioLevels(level: l, bass: d(1) ?? l, mid: d(2) ?? l, melody: d(3) ?? l,
                                      treble: d(4) ?? l, rise: d(5) ?? 0, quiet: d(6) ?? 0,
                                      tempoPhase: d(7) ?? 0, tempoConfidence: d(8) ?? 0))
        case "MODE": return n(0).map { .mode($0) } ?? .unknown(raw)
        case "BRIGHT": return d(0).map { .brightness($0) } ?? .unknown(raw)
        case "HUE": return d(0).map { .hue($0) } ?? .unknown(raw)
        case "CYCLE": return d(0).map { .cycle($0) } ?? .unknown(raw)
        case "NOODLE": return n(0).map { .noodleFollow($0 != 0) } ?? .unknown(raw)
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
    static func noodleFollow(_ on: Bool) -> String { "NOODLE \(on ? 1 : 0)" }
    static let identify = "IDENT"
    static let rollCall = "HELLO"
    static let beat = "BEAT"
    static func hello(id: String, slot: Int) -> String { "HELLO \(id) APP \(slot) app \(BuildInfo.appVersion) \(BuildInfo.modeCount)" }
}
