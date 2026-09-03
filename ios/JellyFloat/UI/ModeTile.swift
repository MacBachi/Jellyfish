import SwiftUI

struct ModeTile: View {
    let mode: JellyMode
    let selected: Bool
    let playing: Bool   // the playlist is currently showing this mode
    var missingOn = 0   // how many jellies in the bloom lack this mode

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 6) {
                ForEach(Array(mode.tintHues.enumerated()), id: \.offset) { _, h in
                    Circle().fill(Theme.color(hue: h, saturation: 0.85, value: 0.95)).frame(width: 9, height: 9)
                        .shadow(color: Theme.color(hue: h).opacity(0.7), radius: 5)
                }
                Spacer()
                if missingOn > 0 {
                    Label("\(missingOn)", systemImage: "exclamationmark.triangle.fill").font(.caption2.weight(.semibold)).foregroundStyle(Theme.amber)
                        .accessibilityLabel("not on \(missingOn) jellies")
                }
                if playing { Image(systemName: "play.fill").font(.caption2).foregroundStyle(Theme.cyan) }
            }
            Spacer(minLength: 4)
            Text(mode.name).font(.headline).foregroundStyle(Theme.ink)
            Text(mode.blurb).font(.caption).foregroundStyle(Theme.inkDim).lineLimit(2).fixedSize(horizontal: false, vertical: true)
        }
        .padding(14)
        .frame(maxWidth: .infinity, minHeight: 108, alignment: .topLeading)
        .background(
            LinearGradient(colors: mode.tintHues.map { Theme.color(hue: $0, saturation: 0.7, value: 0.5).opacity(selected ? 0.45 : 0.16) }, startPoint: .topLeading, endPoint: .bottomTrailing),
            in: RoundedRectangle(cornerRadius: 20, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).strokeBorder(selected ? Theme.cyan.opacity(0.9) : .white.opacity(0.10), lineWidth: selected ? 1.5 : 1))
        .shadow(color: selected ? Theme.cyan.opacity(0.35) : .clear, radius: 14)
        .animation(.easeInOut(duration: 0.25), value: selected)
    }
}
