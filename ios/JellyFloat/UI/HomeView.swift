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
                        SectionTitle(text: group.rawValue)
                        LazyVGrid(columns: columns, spacing: 12) {
                            ForEach(JellyMode.allCases.filter { $0.group == group }) { mode in
                                Button { model.select(mode) } label: {
                                    ModeTile(mode: mode, selected: model.state.mode == mode, playing: model.state.mode == .playlist && model.effectiveMode == mode)
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
            Text(model.slot >= 0 ? "virtual jelly · slot \(model.slot)" : "virtual jelly")
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
