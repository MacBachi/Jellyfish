import SwiftUI

/// The look: the icon's deep-sea navy, neon cyan and magenta, frosted glass cards.
enum Theme {
    static let deepTop = Color(red: 3 / 255, green: 28 / 255, blue: 52 / 255)
    static let deepBottom = Color(red: 8 / 255, green: 8 / 255, blue: 24 / 255)
    static let cyan = Color(red: 0.31, green: 0.89, blue: 1.0)
    static let magenta = Color(red: 1.0, green: 0.37, blue: 0.82)
    static let amber = Color(red: 1.0, green: 0.76, blue: 0.35)
    static let ink = Color.white.opacity(0.92)
    static let inkDim = Color.white.opacity(0.55)

    static var background: some View {
        LinearGradient(colors: [deepTop, deepBottom], startPoint: .top, endPoint: .bottom)
            .ignoresSafeArea()
    }

    static func color(hue: Double, saturation: Double = 1, value: Double = 1) -> Color {
        Color(hue: ((hue.truncatingRemainder(dividingBy: 360) + 360).truncatingRemainder(dividingBy: 360)) / 360, saturation: saturation, brightness: value)
    }
}

struct GlassCard: ViewModifier {
    var padding: CGFloat = 16
    func body(content: Content) -> some View {
        content
            .padding(padding)
            .background(.white.opacity(0.06), in: RoundedRectangle(cornerRadius: 22, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 22, style: .continuous).strokeBorder(.white.opacity(0.10), lineWidth: 1))
    }
}

extension View {
    func glassCard(padding: CGFloat = 16) -> some View { modifier(GlassCard(padding: padding)) }
}

struct SectionTitle: View {
    let text: String
    var body: some View {
        Text(text.uppercased())
            .font(.caption.weight(.semibold))
            .tracking(1.4)
            .foregroundStyle(Theme.inkDim)
            .frame(maxWidth: .infinity, alignment: .leading)
    }
}
