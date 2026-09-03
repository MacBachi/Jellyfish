import SwiftUI

struct SwarmView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                HStack { Text("Bloom").font(.system(size: 30, weight: .semibold, design: .rounded)).foregroundStyle(Theme.ink); Spacer(); StatusPill() }
                HStack(spacing: 12) {
                    Button { model.identify() } label: { Label("Identify", systemImage: "lightbulb.max").frame(maxWidth: .infinity) }
                        .buttonStyle(.borderedProminent).tint(Theme.magenta.opacity(0.8))
                    Button { model.rollCall() } label: { Label("Roll call", systemImage: "hand.wave").frame(maxWidth: .infinity) }
                        .buttonStyle(.bordered).tint(Theme.cyan)
                }
                Text("Identify makes the jelly that runs the network blink red three times; every other jelly blinks blue in step.")
                    .font(.caption).foregroundStyle(Theme.inkDim).frame(maxWidth: .infinity, alignment: .leading)
                VStack(spacing: 10) {
                    SectionTitle(text: "Jellies")
                    if model.roster.isEmpty {
                        Text(model.connection.isLive ? "Nobody has answered yet. Tap roll call." : "Connect to see who is in the bloom.")
                            .font(.subheadline).foregroundStyle(Theme.inkDim).frame(maxWidth: .infinity, alignment: .leading).glassCard()
                    }
                    ForEach(model.roster) { entry in rosterRow(entry) }
                    rosterRow(RosterEntry(id: model.settings.jellyID, role: .app, slot: model.slot, ip: "this phone", lastSeen: Date()), isSelf: true)
                }
                Spacer(minLength: 24)
            }
            .padding(16)
        }
        .background(Theme.background)
    }

    private func rosterRow(_ e: RosterEntry, isSelf: Bool = false) -> some View {
        HStack(spacing: 14) {
            Circle()
                .fill(e.slot >= 0 ? Theme.color(hue: JellyPalette.hue(forSlot: e.slot), saturation: 0.85, value: 0.95) : Color.white.opacity(0.15))
                .frame(width: 34, height: 34)
                .shadow(color: e.slot >= 0 ? Theme.color(hue: JellyPalette.hue(forSlot: e.slot)).opacity(0.6) : .clear, radius: 8)
                .overlay(Text(e.slot >= 0 ? "\(e.slot)" : "?").font(.caption.weight(.bold)).foregroundStyle(.black.opacity(0.7)))
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 6) {
                    Text(isSelf ? "This phone" : "Jelly \(e.id)").font(.headline).foregroundStyle(Theme.ink)
                    Text(roleName(e.role)).font(.caption2.weight(.semibold)).padding(.horizontal, 6).padding(.vertical, 2)
                        .background(e.role == .ap ? Theme.magenta.opacity(0.35) : .white.opacity(0.1), in: Capsule()).foregroundStyle(Theme.ink)
                }
                Text(isSelf ? (e.slot >= 0 ? "virtual jelly, colour slot \(e.slot)" : "virtual jelly, waiting for a colour slot") : "\(e.ip)  ·  \(e.lastSeen.formatted(date: .omitted, time: .standard))")
                    .font(.caption).foregroundStyle(Theme.inkDim)
            }
            Spacer()
        }
        .glassCard(padding: 12)
    }

    private func roleName(_ r: RosterEntry.Role) -> String {
        switch r { case .ap: return "runs the network"; case .station: return "jelly"; case .app: return "app" }
    }
}
