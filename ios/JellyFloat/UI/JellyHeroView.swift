import SwiftUI

/// The virtual jelly: bell, ring of LEDs, four tentacles, four noodle loops, drawn from the
/// engine's frame at 30 frames per second.
struct JellyHeroView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30.0)) { _ in
            let frame = model.renderFrame()
            Canvas { ctx, size in
                draw(frame, in: ctx, size: size)
            }
        }
    }

    private func color(_ c: HSV, alpha: Double = 1) -> Color {
        Color(hue: c.h / 360, saturation: c.s, brightness: max(c.v, 0), opacity: alpha)
    }

    private func draw(_ f: JellyFrame, in ctx: GraphicsContext, size: CGSize) {
        let w = size.width, h = size.height
        let cx = w / 2
        let bellW = w * 0.62, bellH = h * 0.30
        let bellTop = h * 0.10
        let ringY = bellTop + bellH * 0.78
        let ringRx = bellW * 0.46, ringRy = bellH * 0.16

        // average ring colour tints the bell
        let avgV = f.ring.map(\.v).reduce(0, +) / Double(max(f.ring.count, 1))
        let avgH = f.ring.isEmpty ? 200 : f.ring.max(by: { $0.v < $1.v })!.h
        let bellTint = Color(hue: avgH / 360, saturation: 0.55, brightness: 0.35 + 0.65 * avgV)

        // bell glow + body
        let bellRect = CGRect(x: cx - bellW / 2, y: bellTop, width: bellW, height: bellH * 1.3)
        var glow = ctx
        glow.addFilter(.blur(radius: 28))
        glow.fill(Path(ellipseIn: bellRect.insetBy(dx: -10, dy: -10)), with: .color(bellTint.opacity(0.35 + 0.4 * avgV)))
        let bellPath = bellShape(rect: CGRect(x: cx - bellW / 2, y: bellTop, width: bellW, height: bellH))
        ctx.fill(bellPath, with: .linearGradient(Gradient(colors: [bellTint.opacity(0.55), bellTint.opacity(0.12)]), startPoint: CGPoint(x: cx, y: bellTop), endPoint: CGPoint(x: cx, y: bellTop + bellH)))
        ctx.stroke(bellPath, with: .color(.white.opacity(0.35)), lineWidth: 1.2)

        // noodle loops on the bell
        for (n, level) in f.noodles.enumerated() {
            let angle = Double(n) * .pi / 2 + .pi / 4
            let px = cx + cos(angle) * bellW * 0.22, py = bellTop + bellH * 0.45 + sin(angle) * bellH * 0.18
            var loop = Path()
            loop.addArc(center: CGPoint(x: px, y: py), radius: bellW * 0.07, startAngle: .degrees(200), endAngle: .degrees(340), clockwise: false)
            var g = ctx; g.addFilter(.blur(radius: 6))
            g.stroke(loop, with: .color(Theme.magenta.opacity(0.9 * level)), lineWidth: 5)
            ctx.stroke(loop, with: .color(Color(hue: 0.9, saturation: 0.5, brightness: 0.4 + 0.6 * level)), lineWidth: 2)
        }

        // ring of LEDs, an ellipse seen from slightly above
        let n = f.ring.count
        for (i, c) in f.ring.enumerated() {
            let a = 2 * Double.pi * Double(i) / Double(n) + .pi / 2
            let p = CGPoint(x: cx + cos(a) * ringRx, y: ringY + sin(a) * ringRy)
            drawLED(ctx, at: p, c, size: 3.2, glow: 9)
        }

        // tentacles hanging from four points, with a gentle sway; LED dots along them
        let t0 = Date().timeIntervalSinceReferenceDate
        let anchorsX: [CGFloat] = [-0.30, -0.11, 0.11, 0.30]
        for (t, leds) in f.tentacles.enumerated() where t < 4 {
            let ax = cx + anchorsX[t] * bellW
            let ay = ringY + ringRy * 0.6
            let length = h - ay - 24
            let sway = CGFloat(sin(t0 * 0.6 + Double(t) * 1.3)) * bellW * 0.08
            let c1 = CGPoint(x: ax + sway, y: ay + length * 0.4)
            let c2 = CGPoint(x: ax - sway * 0.6, y: ay + length * 0.75)
            let end = CGPoint(x: ax + sway * 0.4, y: ay + length)
            var path = Path(); path.move(to: CGPoint(x: ax, y: ay)); path.addCurve(to: end, control1: c1, control2: c2)
            ctx.stroke(path, with: .color(.white.opacity(0.18)), lineWidth: 1.2)
            let m = leds.count
            for (j, c) in leds.enumerated() {
                let u = (Double(j) + 0.6) / Double(m)
                let p = bezier(CGPoint(x: ax, y: ay), c1, c2, end, CGFloat(u))
                drawLED(ctx, at: p, c, size: 2.6, glow: 7)
            }
        }
    }

    private func drawLED(_ ctx: GraphicsContext, at p: CGPoint, _ c: HSV, size: CGFloat, glow: CGFloat) {
        guard c.v > 0.004 else {
            ctx.fill(Path(ellipseIn: CGRect(x: p.x - 1, y: p.y - 1, width: 2, height: 2)), with: .color(.white.opacity(0.08)))
            return
        }
        var g = ctx; g.addFilter(.blur(radius: glow * CGFloat(0.4 + c.v)))
        g.fill(Path(ellipseIn: CGRect(x: p.x - glow, y: p.y - glow, width: glow * 2, height: glow * 2)), with: .color(color(c, alpha: 0.55 * c.v)))
        ctx.fill(Path(ellipseIn: CGRect(x: p.x - size, y: p.y - size, width: size * 2, height: size * 2)), with: .color(color(HSV(h: c.h, s: c.s * 0.8, v: 0.3 + 0.7 * c.v))))
    }

    private func bellShape(rect r: CGRect) -> Path {
        var p = Path()
        p.move(to: CGPoint(x: r.minX, y: r.maxY))
        p.addCurve(to: CGPoint(x: r.midX, y: r.minY), control1: CGPoint(x: r.minX, y: r.minY + r.height * 0.15), control2: CGPoint(x: r.minX + r.width * 0.25, y: r.minY))
        p.addCurve(to: CGPoint(x: r.maxX, y: r.maxY), control1: CGPoint(x: r.maxX - r.width * 0.25, y: r.minY), control2: CGPoint(x: r.maxX, y: r.minY + r.height * 0.15))
        p.addQuadCurve(to: CGPoint(x: r.minX, y: r.maxY), control: CGPoint(x: r.midX, y: r.maxY + r.height * 0.28))
        return p
    }

    private func bezier(_ p0: CGPoint, _ p1: CGPoint, _ p2: CGPoint, _ p3: CGPoint, _ t: CGFloat) -> CGPoint {
        let u = 1 - t
        let x = u*u*u*p0.x + 3*u*u*t*p1.x + 3*u*t*t*p2.x + t*t*t*p3.x
        let y = u*u*u*p0.y + 3*u*u*t*p1.y + 3*u*t*t*p2.y + t*t*t*p3.y
        return CGPoint(x: x, y: y)
    }
}
