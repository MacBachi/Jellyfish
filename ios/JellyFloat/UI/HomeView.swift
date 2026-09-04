import SwiftUI

struct HomeView: View {
    @EnvironmentObject private var model: AppModel
    private let columns = [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)]

    var body: some View {
        ScrollView {
            VStack(spacing: 18) {
                header
                JellyHeroView()
                    .frame(height: 340)
                    .overlay(alignment: .bottomTrailing) { heroCaption }
                nowPlaying
                ControlsView()
                ForEach(JellyMode.Group.allCases) { group in
                    VStack(spacing: 10) {
                        SectionTitle(text: LocalizedStringKey(group.rawValue))
                        LazyVGrid(columns: columns, spacing: 12) {
                            ForEach(JellyMode.allCases.filter { $0.group == group }) { mode in
                                Button { model.select(mode) } label: {
                                    ModeTile(mode: mode, selected: model.state.mode == mode, playing: model.state.mode == .playlist && model.effectiveMode == mode,
                                             missingOn: model.jelliesLacking(mode).count)
                                }
                                .buttonStyle(.plain)
                            }
                        }
                    }
                }
                Spacer(minLength: 24)
            }
            .padding(.horizontal, 16)
            .padding(.top, 8)
        }
        .background(Theme.background)
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            Text("JellyFloat").font(.system(size: 30, weight: .semibold, design: .rounded)).foregroundStyle(Theme.ink)
            Spacer()
            StatusPill()
        }
    }

    private var heroCaption: some View {
        VStack(alignment: .trailing, spacing: 2) {
            if model.slot >= 0 { Text("virtual jelly · slot \(model.slot)") } else { Text("virtual jelly") }
            if model.state.mode == .playlist { Text("playlist · \(model.effectiveMode.name)") }
        }
        .font(.caption2).foregroundStyle(Theme.inkDim).padding(8)
    }

    private var nowPlaying: some View {
        HStack(spacing: 14) {
            Button { model.stepMode(-1) } label: { Image(systemName: "backward.fill") }
            VStack(spacing: 2) {
                Text(model.state.mode.name).font(.title3.weight(.semibold)).foregroundStyle(Theme.ink)
                Text(model.state.mode.blurb).font(.caption).foregroundStyle(Theme.inkDim).lineLimit(1)
            }
            .frame(maxWidth: .infinity)
            Button { model.stepMode(+1) } label: { Image(systemName: "forward.fill") }
        }
        .font(.title3)
        .foregroundStyle(Theme.cyan)
        .glassCard(padding: 14)
        .overlay(alignment: .bottom) {
            if !model.jelliesLacking(model.state.mode).isEmpty {
                Text(lackingNote(for: model.state.mode)).font(.caption2).foregroundStyle(Theme.amber).lineLimit(2)
                    .multilineTextAlignment(.center).padding(.horizontal, 12).offset(y: 18)
            }
        }
        .padding(.bottom, model.jelliesLacking(model.state.mode).isEmpty ? 0 : 22)
    }

    /// "Jelly 1b3c doesn't know SOS and shows Breathe instead."
    private func lackingNote(for mode: JellyMode) -> String {
        let lacking = model.jelliesLacking(mode)
        var names = lacking.prefix(3).map { String(localized: "Jelly \($0.id)") }.joined(separator: ", ")
        if lacking.count > 3 { names += String(localized: " and \(lacking.count - 3) more") }
        let fb = lacking.first?.fallback(for: mode)
        let sameFallback = fb != nil && lacking.allSatisfy { $0.fallback(for: mode) == fb }
        if lacking.count == 1 {
            return sameFallback ? String(localized: "\(names) doesn't know \(mode.name) and shows \(fb!.name) instead.")
                                : String(localized: "\(names) doesn't know \(mode.name) and shows another mode instead.")
        }
        return sameFallback ? String(localized: "\(names) don't know \(mode.name) and show \(fb!.name) instead.")
                            : String(localized: "\(names) don't know \(mode.name) and show another mode instead.")
    }
}

struct StatusPill: View {
    @EnvironmentObject private var model: AppModel
    var body: some View {
        HStack(spacing: 6) {
            Circle().fill(model.connection.isLive ? Theme.cyan : Theme.inkDim).frame(width: 7, height: 7)
                .shadow(color: model.connection.isLive ? Theme.cyan : .clear, radius: 4)
            Text(model.connection.label).font(.caption.weight(.medium)).lineLimit(1)
        }
        .foregroundStyle(Theme.ink)
        .padding(.horizontal, 10).padding(.vertical, 6)
        .background(.white.opacity(0.08), in: Capsule())
    }
}
