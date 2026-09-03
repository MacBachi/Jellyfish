import Foundation

// A port of the firmware's effects (firmware_cpp/jell_effects.cpp, jell_field.cpp, JellyFloatOS.cpp)
// for the virtual jelly. Same noise, same constants, same master time, so the phone's jelly
// moves in step with the real ones.

struct HSV {
    var h: Double
    var s: Double
    var v: Double
    static let black = HSV(h: 0, s: 0, v: 0)
}

struct JellyFrame {
    var ring: [HSV]
    var tentacles: [[HSV]]
    var noodles: [Double]
}

/// Where the LEDs are, in the firmware's coordinate system: the ring on the unit circle at z = 0,
/// tentacles hanging below.
struct JellyLayout {
    let ring: [SIMD3<Double>]
    let tentacles: [[SIMD3<Double>]]
    let noodles: [SIMD3<Double>] = [
        SIMD3(0.5, 0, 0.8), SIMD3(0, 0.5, 0.8), SIMD3(-0.5, 0, 0.8), SIMD3(0, -0.5, 0.8),
    ]

    static let heightMap: [Double] = [-1, -2, -2.75, -1.75, -0.5, -0.5, -1.5, -2.5, -3.5, -4.5, -5.5, -6.5, -7.5, -8.5, -9.5, -10.5]
    static let tentacleXY: [(Double, Double)] = [
        (1, 0), (0, 1), (-1, 0), (0, -1), (0.7071, 0.7071), (-0.7071, 0.7071), (-0.7071, -0.7071),
    ]
    /// The firmware's configured maximum; drops run this far even on shorter tentacles.
    static let tentacleLEDsConfigured = 16

    init(ringLEDs: Int, tentacleCount: Int = 4, tentacleLEDs: Int = 12) {
        ring = (0..<ringLEDs).map { i in
            let a = 2 * Double.pi * Double(i) / Double(ringLEDs)
            return SIMD3(cos(a), sin(a), 0)
        }
        tentacles = (0..<tentacleCount).map { t in
            (0..<tentacleLEDs).map { j in
                SIMD3(JellyLayout.tentacleXY[t].0, JellyLayout.tentacleXY[t].1, JellyLayout.heightMap[j])
            }
        }
    }
}

/// jell_field.cpp: 3D value noise in [0, 1].
enum Field {
    private static func hash(_ x: Int, _ y: Int, _ z: Int) -> Double {
        var h = UInt32(bitPattern: Int32(truncatingIfNeeded: x)) &* 374761393
        h = h &+ UInt32(bitPattern: Int32(truncatingIfNeeded: y)) &* 668265263
        h = h &+ UInt32(bitPattern: Int32(truncatingIfNeeded: z)) &* 2147483647
        h = (h ^ (h >> 13)) &* 1274126177
        h ^= h >> 16
        return Double(h) / 4294967295.0
    }
    private static func fade(_ t: Double) -> Double { t * t * t * (t * (t * 6 - 15) + 10) }
    private static func lerp(_ a: Double, _ b: Double, _ t: Double) -> Double { a + t * (b - a) }

    static func noise(_ p: SIMD3<Double>, scale: Double = 1, time: Double = 0) -> Double {
        let x = p.x * scale + time, y = p.y * scale, z = p.z * scale
        let x0 = Int(floor(x)), y0 = Int(floor(y)), z0 = Int(floor(z))
        let tx = fade(x - Double(x0)), ty = fade(y - Double(y0)), tz = fade(z - Double(z0))
        let i00 = lerp(hash(x0, y0, z0), hash(x0 + 1, y0, z0), tx)
        let i10 = lerp(hash(x0, y0 + 1, z0), hash(x0 + 1, y0 + 1, z0), tx)
        let i01 = lerp(hash(x0, y0, z0 + 1), hash(x0 + 1, y0, z0 + 1), tx)
        let i11 = lerp(hash(x0, y0 + 1, z0 + 1), hash(x0 + 1, y0 + 1, z0 + 1), tx)
        return lerp(lerp(i00, i10, ty), lerp(i01, i11, ty), tz)
    }
}

func hueLerpShortest(_ a: Double, _ b: Double, _ t: Double) -> Double {
    let d = (b - a + 540).truncatingRemainder(dividingBy: 360) - 180
    return (a + d * t + 360).truncatingRemainder(dividingBy: 360)
}

private func smoothstep01(_ t: Double) -> Double { let c = min(max(t, 0), 1); return c * c * (3 - 2 * c) }
private func frac(_ x: Double) -> Double { x - floor(x) }
private func bump(_ p: Double) -> Double { 0.5 - 0.5 * cos(2 * .pi * p) }

/// jell_effects.cpp palette_hue()
func paletteHue(slot: Int, time: Double, cyclePeriod: Double, cycle: Bool) -> Double {
    let n = JellyPalette.hues.count
    let s = max(slot, 0)
    if !cycle { return JellyPalette.hues[s % n] }
    let period = max(cyclePeriod, 1)
    let blend = min(2.0, period * 0.5)
    let phase = time / period
    let step = Int(floor(phase))
    let within = (phase - Double(step)) * period
    let from = (((s + step) % n) + n) % n
    let to = (from + 1) % n
    let t = (within - (period - blend)) / blend
    return hueLerpShortest(JellyPalette.hues[from], JellyPalette.hues[to], smoothstep01(t))
}

final class JellyEngine {
    struct Input {
        var mode: JellyMode
        var masterUs: Int64
        var slot: Int
        var cyclePeriod: Double
        var brightness: Double
        var hueOffset: Double
        var level: Double          // the AP's smoothed microphone level
        var beat: Bool
        var identStartUs: Int64    // 0 = none
        var isAP: Bool
    }

    let layout: JellyLayout
    private var ring: [HSV]
    private var tentacles: [[HSV]]
    private var noodles: [Double]

    // crossfade (JellyFloatOS.cpp core1_entry)
    private var snapRing: [HSV], snapTentacles: [[HSV]], snapNoodles: [Double]
    private var shown: JellyMode?
    private var fading = false
    private var fadeStartUs: Int64 = 0
    private var mix = 1.0

    // effect state
    private var lastUs: Int64 = 0
    private var sinceBeat = 1000.0
    private var sinceIdleDrop = 0.0
    private var dropPos: [Double] = Array(repeating: -1, count: 7)
    private var drizzlePos: [Double] = Array(repeating: -1, count: 7)
    private var glimmerPool = [Spark](repeating: Spark(), count: 12)
    private var fireflyPool = [Spark](repeating: Spark(), count: 6)
    private var whisperEMA = 0.0
    private var testState = 0
    private var testLastStepUs: Int64 = 0

    static let playlist: [JellyMode] = [.breathe, .glimmer, .aurora, .current, .lantern, .moonlight, .drizzle, .fireflies, .swarm, .paletteCycle]
    static let playlistStepS = 180.0
    static let crossfadeS = 1.0

    init(layout: JellyLayout) {
        self.layout = layout
        ring = Array(repeating: .black, count: layout.ring.count)
        tentacles = layout.tentacles.map { Array(repeating: HSV.black, count: $0.count) }
        noodles = Array(repeating: 0, count: layout.noodles.count)
        snapRing = ring; snapTentacles = tentacles; snapNoodles = noodles
    }

    static func effectiveMode(_ mode: JellyMode, masterUs: Int64) -> JellyMode {
        guard mode == .playlist else { return mode }
        let step = Int64(playlistStepS * 1e6)
        let n = Int64(playlist.count)
        let index = Int(((masterUs / step) % n + n) % n)
        return playlist[index]
    }

    // MARK: - frame

    func render(_ input: Input) -> JellyFrame {
        let nowUs = TimeSync.nowUs()
        var dt = lastUs == 0 ? 0 : Double(nowUs - lastUs) * 1e-6
        lastUs = nowUs
        dt = min(dt, 0.1)
        let time = Double(input.masterUs) * 1e-6
        let mode = JellyEngine.effectiveMode(input.mode, masterUs: input.masterUs)

        if renderIdent(input) {
            shown = mode; fading = false; mix = 1
            return output(brightness: 1, hueOffset: 0, mix: 1)
        }

        if mode != shown {
            if shown != nil {
                beginCrossfade(currentMix: fading ? mix : 1)
                fading = true; fadeStartUs = nowUs; mix = 0
            }
            shown = mode
        }
        if fading {
            let t = Double(nowUs - fadeStartUs) * 1e-6 / JellyEngine.crossfadeS
            if t >= 1 { fading = false; mix = 1 } else { mix = smoothstep01(t) }
        }

        switch mode {
        case .breathe: breathe(time)
        case .glimmer: glimmer(dt)
        case .aurora: aurora(time)
        case .current: current(time)
        case .lantern: lantern(time)
        case .moonlight: moonlight(time)
        case .drizzle: drizzle(dt)
        case .fireflies: fireflies(dt)
        case .swarm: swarm(time, slot: input.slot)
        case .whisper: whisper(time, level: input.level, dt: dt)
        case .micField: micField(time, level: input.level)
        case .drops: drops(time, level: input.level, beat: input.beat, dt: dt)
        case .palette: ambient(time, noiseScale: 1, hueBase: paletteHue(slot: input.slot, time: time, cyclePeriod: input.cyclePeriod, cycle: false), hueRange: 20, timeScale: 0.15)
        case .paletteCycle: ambient(time, noiseScale: 1, hueBase: paletteHue(slot: input.slot, time: time, cyclePeriod: input.cyclePeriod, cycle: true), hueRange: 20, timeScale: 0.15)
        case .ambientRainbow: ambient(time, noiseScale: 1, hueBase: 220, hueRange: 360, timeScale: 0.15)
        case .ambientDeepSea: ambient(time, noiseScale: 2, hueBase: 220, hueRange: 100, timeScale: 0.8)
        case .micLevelCheck: all(HSV(h: 220, s: 1, v: input.level)); allNoodles(input.level)
        case .ledChannelTest: channelTest(nowUs)
        case .playlist: clear()
        }
        return output(brightness: input.brightness, hueOffset: input.hueOffset, mix: mix)
    }

    // MARK: - buffers, fade, crossfade (jell_led.hpp / jell_canvas.cpp)

    private func clear() { all(.black); allNoodles(0) }

    private func all(_ c: HSV) {
        for i in ring.indices { ring[i] = c }
        for t in tentacles.indices { for j in tentacles[t].indices { tentacles[t][j] = c } }
    }
    private func allNoodles(_ v: Double) { for i in noodles.indices { noodles[i] = min(max(v, 0), 1) } }
    private func setRing(_ i: Int, _ h: Double, _ s: Double, _ v: Double) {
        ring[i] = HSV(h: (h.truncatingRemainder(dividingBy: 360) + 360).truncatingRemainder(dividingBy: 360), s: min(max(s, 0), 1), v: min(max(v, 0), 1))
    }
    private func setSpoke(_ t: Int, _ j: Int, _ h: Double, _ s: Double, _ v: Double) {
        guard t < tentacles.count, j >= 0, j < tentacles[t].count else { return }
        tentacles[t][j] = HSV(h: (h.truncatingRemainder(dividingBy: 360) + 360).truncatingRemainder(dividingBy: 360), s: min(max(s, 0), 1), v: min(max(v, 0), 1))
    }
    private func setNoodle(_ n: Int, _ v: Double) { if n < noodles.count { noodles[n] = min(max(v, 0), 1) } }

    private func fade(dt: Double, tau: Double) {
        let f = exp(-dt / tau)
        for i in ring.indices { ring[i].v *= f; if ring[i].v < 0.001 { ring[i].v = 0 } }
        for t in tentacles.indices { for j in tentacles[t].indices { tentacles[t][j].v *= f; if tentacles[t][j].v < 0.001 { tentacles[t][j].v = 0 } } }
    }

    private func blended(_ a: HSV, _ b: HSV, _ mix: Double) -> HSV {
        if mix >= 1 { return b }
        let denom = (1 - mix) * a.v + mix * b.v
        let w = denom > 1e-4 ? (mix * b.v) / denom : mix
        return HSV(h: hueLerpShortest(a.h, b.h, w), s: a.s + (b.s - a.s) * mix, v: a.v + (b.v - a.v) * mix)
    }

    private func beginCrossfade(currentMix: Double) {
        for i in ring.indices { snapRing[i] = blended(snapRing[i], ring[i], currentMix); ring[i] = .black }
        for t in tentacles.indices { for j in tentacles[t].indices { snapTentacles[t][j] = blended(snapTentacles[t][j], tentacles[t][j], currentMix); tentacles[t][j] = .black } }
        for n in noodles.indices { snapNoodles[n] = snapNoodles[n] + (noodles[n] - snapNoodles[n]) * currentMix; noodles[n] = 0 }
    }

    private func output(brightness: Double, hueOffset: Double, mix: Double) -> JellyFrame {
        let b = min(max(brightness, 0), 1)
        func out(_ a: HSV, _ live: HSV) -> HSV {
            var c = blended(a, live, mix)
            c.h = (c.h + hueOffset + 360).truncatingRemainder(dividingBy: 360)
            c.v *= b
            return c
        }
        return JellyFrame(
            ring: ring.indices.map { out(snapRing[$0], ring[$0]) },
            tentacles: tentacles.indices.map { t in tentacles[t].indices.map { out(snapTentacles[t][$0], tentacles[t][$0]) } },
            noodles: noodles.indices.map { (snapNoodles[$0] + (noodles[$0] - snapNoodles[$0]) * mix) * b }
        )
    }

    private func renderIdent(_ input: Input) -> Bool {
        guard input.identStartUs != 0 else { return false }
        let period: Int64 = 500_000, blinks: Int64 = 3
        let elapsed = input.masterUs - input.identStartUs
        guard elapsed >= 0, elapsed < period * blinks else { return false }
        let on = (elapsed % period) < period / 2
        all(HSV(h: input.isAP ? 0 : 240, s: 1, v: on ? 1 : 0))
        allNoodles(on ? 1 : 0)
        return true
    }

    // MARK: - helpers shared by effects

    struct Spark {
        var active = false
        var spoke = -1
        var pixel = 0
        var age = 0.0
        var life = 1.0
        var hue = 0.0
        var peak = 1.0
    }

    private func spawnSpark(_ pool: inout [Spark], ringShare: Double, life: ClosedRange<Double>, hue: ClosedRange<Double>, peak: ClosedRange<Double>) {
        guard let i = pool.firstIndex(where: { !$0.active }) else { return }
        var k = Spark()
        k.active = true
        if Double.random(in: 0..<1) < ringShare {
            k.spoke = -1; k.pixel = Int.random(in: 0..<layout.ring.count)
        } else {
            k.spoke = Int.random(in: 0..<layout.tentacles.count); k.pixel = Int.random(in: 0..<layout.tentacles[k.spoke].count)
        }
        k.life = Double.random(in: life); k.hue = Double.random(in: hue); k.peak = Double.random(in: peak)
        pool[i] = k
    }

    private func drawSparks(_ pool: inout [Spark], dt: Double, envelope: (Double) -> Double, saturation: Double, bgV: Double) {
        for i in pool.indices where pool[i].active {
            pool[i].age += dt
            if pool[i].age >= pool[i].life { pool[i].active = false; continue }
            let v = max(bgV, pool[i].peak * envelope(pool[i].age / pool[i].life))
            if pool[i].spoke < 0 { setRing(pool[i].pixel, pool[i].hue, saturation, v) } else { setSpoke(pool[i].spoke, pool[i].pixel, pool[i].hue, saturation, v) }
        }
    }

    private func advanceDrops(_ pos: inout [Double], speed: Double, dt: Double, h: Double, s: Double, v: Double) {
        for t in pos.indices where pos[t] >= 0 {
            setSpoke(t, Int(pos[t]), h, s, v)
            pos[t] += speed * dt
            if pos[t] >= Double(JellyLayout.tentacleLEDsConfigured) { pos[t] = -1 }
        }
    }

    // MARK: - the effects (same constants as jell_effects.cpp)

    private func ambient(_ time: Double, noiseScale: Double, hueBase: Double, hueRange: Double, timeScale: Double) {
        let off = 1000.0
        for (i, p) in layout.ring.enumerated() {
            let fb = Field.noise(p, scale: noiseScale, time: time * timeScale)
            let fh = Field.noise(SIMD3(p.x + off, p.y, p.z), scale: noiseScale, time: time * timeScale)
            setRing(i, hueBase + (fh * fh - 0.5) * hueRange, 1, fb)
        }
        for (t, tent) in layout.tentacles.enumerated() {
            for (j, p) in tent.enumerated() {
                let fb = Field.noise(p, scale: noiseScale, time: time * timeScale)
                let fh = Field.noise(SIMD3(p.x + off, p.y, p.z), scale: noiseScale, time: time * timeScale)
                setSpoke(t, j, hueBase + (fh * fh - 0.5) * hueRange, 1, fb)
            }
        }
        for (n, p) in layout.noodles.enumerated() {
            setNoodle(n, Field.noise(p, scale: noiseScale, time: time * timeScale) * 0.6 + 0.4)
        }
    }

    private func micField(_ time: Double, level: Double) {
        for (i, p) in layout.ring.enumerated() {
            let n = Field.noise(p, scale: 1, time: level + time * 0.3)
            setRing(i, 220 + n * n * 100, 1, level)
        }
        for (t, tent) in layout.tentacles.enumerated() {
            for (j, p) in tent.enumerated() {
                let n = Field.noise(p, scale: 0.5, time: level + time * 0.3)
                setSpoke(t, j, 220 + n * n * 100, 1, level)
            }
        }
        allNoodles(min(level * 2, 1))
    }

    private func drops(_ time: Double, level: Double, beat: Bool, dt: Double) {
        sinceBeat += dt; sinceIdleDrop += dt
        if beat {
            sinceBeat = 0; sinceIdleDrop = 0
            for t in dropPos.indices { dropPos[t] = 0 }
        } else if sinceIdleDrop > 1.5 {
            sinceIdleDrop = 0
            dropPos[Int.random(in: 0..<dropPos.count)] = 0
        }
        fade(dt: dt, tau: 0.12)
        let flash = exp(-sinceBeat / 0.25)
        let ringV = max(0.05 + 0.35 * level, flash)
        for (i, p) in layout.ring.enumerated() {
            let n = Field.noise(p, scale: 1, time: time * 0.3)
            setRing(i, 220 + (190 - 220) * flash + n * 30, 1, ringV)
        }
        advanceDrops(&dropPos, speed: 30, dt: dt, h: 190, s: 0.8, v: 1)
        allNoodles(max(0.15, flash))
    }

    private func breathe(_ time: Double) {
        let p = frac(time / 6), e = bump(p)
        for i in layout.ring.indices { setRing(i, 200 + 15 * e, 0.85, 0.06 + 0.54 * e) }
        for (t, tent) in layout.tentacles.enumerated() {
            for (j, pos) in tent.enumerated() {
                let ez = bump(frac(p + 0.25 * pos.z / 10.5))
                setSpoke(t, j, 200 + 15 * ez, 0.85, 0.06 + 0.38 * ez)
            }
        }
        allNoodles(0.10 + 0.5 * e)
    }

    private func glimmer(_ dt: Double) {
        all(HSV(h: 190, s: 0.9, v: 0.02)); allNoodles(0.03)
        if Double.random(in: 0..<1) < dt * 2 { spawnSpark(&glimmerPool, ringShare: 0.3, life: 1.5...3, hue: 160...195, peak: 0.5...0.9) }
        drawSparks(&glimmerPool, dt: dt, envelope: { u in u < 0.15 ? u / 0.15 : pow((1 - u) / 0.85, 2) }, saturation: 0.9, bgV: 0.02)
    }

    private func aurora(_ time: Double) {
        let scale = 1.5, ts = 0.03, off = 1000.0
        for (i, p) in layout.ring.enumerated() {
            let fb = Field.noise(p, scale: scale, time: time * ts)
            let fh = Field.noise(SIMD3(p.x + off, p.y, p.z), scale: scale, time: time * ts)
            setRing(i, 195 + (fh - 0.5) * 150, 1, 0.15 + 0.5 * fb)
        }
        for (t, tent) in layout.tentacles.enumerated() {
            for (j, p) in tent.enumerated() {
                let fb = Field.noise(p, scale: scale, time: time * ts)
                let fh = Field.noise(SIMD3(p.x + off, p.y, p.z), scale: scale, time: time * ts)
                setSpoke(t, j, 195 + (fh - 0.5) * 150, 1, 0.35 * (0.3 + 0.7 * fb) * (1 + p.z / 14))
            }
        }
        for (n, p) in layout.noodles.enumerated() { setNoodle(n, 0.15 + 0.25 * Field.noise(p, scale: scale, time: time * ts)) }
    }

    private func current(_ time: Double) {
        let ph = time / 8
        for i in layout.ring.indices { setRing(i, 200, 0.9, 0.12 + 0.08 * sin(2 * .pi * ph)) }
        for (t, tent) in layout.tentacles.enumerated() {
            for j in tent.indices {
                let w = 0.5 + 0.5 * sin(2 * .pi * (ph + Double(j) / 16 + 0.13 * Double(t)))
                setSpoke(t, j, 185, 0.9, 0.08 + 0.35 * w * w)
            }
        }
        for n in layout.noodles.indices { setNoodle(n, 0.10 + 0.15 * (0.5 + 0.5 * sin(2 * .pi * ph + 0.25 * Double(n)))) }
    }

    private func lantern(_ time: Double) {
        func v(_ p: SIMD3<Double>) -> Double {
            let slow = Field.noise(SIMD3(p.x + 1000, p.y, p.z), scale: 0.5, time: time * 0.1)
            let fast = Field.noise(p, scale: 2, time: time * 0.8)
            return 0.35 + 0.15 * slow + 0.08 * (fast - 0.5)
        }
        for (i, p) in layout.ring.enumerated() { setRing(i, 32, 0.95, v(p)) }
        for (t, tent) in layout.tentacles.enumerated() { for (j, p) in tent.enumerated() { setSpoke(t, j, 32 + 14 * p.z / 10.5, 0.95, 0.6 * v(p)) } }
        for (n, p) in layout.noodles.enumerated() { setNoodle(n, 0.35 + 0.1 * Field.noise(p, scale: 2, time: time * 0.8)) }
    }

    private func moonlight(_ time: Double) {
        for (i, p) in layout.ring.enumerated() { setRing(i, 215, 0.35, 0.06 + 0.03 * Field.noise(p, scale: 1, time: time * 0.02)) }
        for (t, tent) in layout.tentacles.enumerated() { for (j, p) in tent.enumerated() { setSpoke(t, j, 215, 0.35, 0.03 + 0.02 * Field.noise(p, scale: 1, time: time * 0.02)) } }
        allNoodles(0.02)
    }

    private func drizzle(_ dt: Double) {
        fade(dt: dt, tau: 0.6)
        for i in layout.ring.indices { setRing(i, 205, 0.8, 0.03) }
        if Double.random(in: 0..<1) < dt * 0.7 {
            let t = Int.random(in: 0..<drizzlePos.count)
            if drizzlePos[t] < 0 { drizzlePos[t] = 0 }
        }
        advanceDrops(&drizzlePos, speed: 6, dt: dt, h: 205, s: 0.6, v: 0.7)
        allNoodles(0.05)
    }

    private func fireflies(_ dt: Double) {
        all(HSV(h: 50, s: 0.8, v: 0)); for i in layout.ring.indices { setRing(i, 50, 0.8, 0.03) }
        allNoodles(0.03)
        if Double.random(in: 0..<1) < dt * 0.8 { spawnSpark(&fireflyPool, ringShare: 0, life: 3...6, hue: 70...85, peak: 0.6...1) }
        drawSparks(&fireflyPool, dt: dt, envelope: { sin(.pi * $0) }, saturation: 0.85, bgV: 0)
    }

    private func swarm(_ time: Double, slot: Int) {
        let hue = paletteHue(slot: slot, time: time, cyclePeriod: 10, cycle: false)
        let q = frac(time / 12 - Double(max(slot, 0)) * 0.125)
        func pulse(_ qq: Double) -> Double { let f = frac(qq); return f < 0.35 ? 0.5 - 0.5 * cos(2 * .pi * f / 0.35) : 0 }
        let g = pulse(q)
        for i in layout.ring.indices { setRing(i, hue, 1, 0.08 + 0.6 * g) }
        for (t, tent) in layout.tentacles.enumerated() { for (j, p) in tent.enumerated() { setSpoke(t, j, hue, 1, 0.05 + 0.5 * pulse(q - 0.05 * (-p.z) / 10.5)) } }
        allNoodles(0.05 + 0.5 * g)
    }

    private func whisper(_ time: Double, level: Double, dt: Double) {
        let l = min(max(level, 0), 1)
        let tau = l > whisperEMA ? 2.0 : 4.0
        whisperEMA += (l - whisperEMA) * (1 - exp(-dt / tau))
        let hue = 200 - 160 * smoothstep01(whisperEMA)
        let base = 0.10 + 0.55 * whisperEMA
        for (i, p) in layout.ring.enumerated() { let n = Field.noise(p, scale: 1, time: time * 0.05) - 0.5; setRing(i, hue + 16 * n, 0.9, base + 0.05 * n) }
        for (t, tent) in layout.tentacles.enumerated() { for (j, p) in tent.enumerated() { let n = Field.noise(p, scale: 1, time: time * 0.05) - 0.5; setSpoke(t, j, hue + 16 * n, 0.9, 0.7 * (base + 0.05 * n)) } }
        allNoodles(0.05 + 0.4 * whisperEMA)
    }

    private func channelTest(_ nowUs: Int64) {
        if nowUs - testLastStepUs >= 1_000_000 { testState += 1; testLastStepUs = nowUs }
        all(HSV(h: [0, 120, 240][testState % 3], s: 1, v: 1))
        for n in noodles.indices { setNoodle(n, n == testState % 4 ? 1 : 0) }
    }
}
