import SwiftUI

struct ControlsView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        VStack(spacing: 18) {
            // brightness
            VStack(alignment: .leading, spacing: 6) {
                HStack { Label("Brightness", systemImage: "sun.max").font(.subheadline.weight(.medium)); Spacer(); Text("\(Int(model.state.brightness * 100)) %").monospacedDigit().foregroundStyle(Theme.inkDim) }
                Slider(value: Binding(get: { model.state.brightness }, set: { model.setBrightness($0) }), in: 0...1)
                    .tint(Theme.cyan)
            }
            // hue
            VStack(alignment: .leading, spacing: 6) {
                HStack { Label("Colour shift", systemImage: "paintpalette").font(.subheadline.weight(.medium)); Spacer(); Text("\(Int(model.state.hueOffset))°").monospacedDigit().foregroundStyle(Theme.inkDim) }
                ZStack {
                    RoundedRectangle(cornerRadius: 4).fill(LinearGradient(colors: stride(from: 0.0, through: 360.0, by: 30).map { Theme.color(hue: $0, saturation: 0.8, value: 0.9) }, startPoint: .leading, endPoint: .trailing)).frame(height: 6).padding(.horizontal, 12)
                    Slider(value: Binding(get: { model.state.hueOffset }, set: { model.setHue($0) }), in: 0...359).tint(.clear)
                }
            }
            // cycle
            HStack {
                Label("Colour cycle", systemImage: "arrow.triangle.2.circlepath").font(.subheadline.weight(.medium))
                Spacer()
                Stepper(value: Binding(get: { model.state.cyclePeriod }, set: { model.setCycle($0) }), in: 2...120, step: model.state.cyclePeriod < 20 ? 1 : 10) {
                    Text("every \(Int(model.state.cyclePeriod)) s").monospacedDigit().foregroundStyle(Theme.inkDim)
                }
            }
        }
        .foregroundStyle(Theme.ink)
        .glassCard()
    }
}
