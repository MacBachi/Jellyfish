import SwiftUI

struct SwarmView: View {
    @EnvironmentObject private var model: AppModel

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
                        (model.connection.isLive ? Text("Nobody has answered yet. Tap roll call.") : Text("Connect to see who is in the bloom."))
                            .font(.subheadline).foregroundStyle(Theme.inkDim).frame(maxWidth: .infinity, alignment: .leading).glassCard()
                    }
                    ForEach(model.roster) { entry in rosterRow(entry) }
                    rosterRow(RosterEntry(id: model.settings.jellyID, role: .app, slot: model.slot, ip: String(localized: "this phone"), lastSeen: Date(),
                                          firmware: BuildInfo.appVersion, modeCount: BuildInfo.modeCount), isSelf: true)
                }
                VStack(spacing: 10) {
                    SectionTitle(text: "Firmware")
                    firmwareCard
                }
                Spacer(minLength: 24)
            }
            .padding(16)
        }
        .background(Theme.background)
    }

    // MARK: firmware overview

    private var firmwareCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Newest known").font(.subheadline).foregroundStyle(Theme.inkDim)
                Spacer()
                Text(BuildInfo.latestFirmware.text).font(.subheadline.weight(.semibold)).foregroundStyle(Theme.ink)
            }
            HStack {
                Text("This app").font(.subheadline).foregroundStyle(Theme.inkDim)
                Spacer()
                Text("\(BuildInfo.appVersion) · \(BuildInfo.modeCount) modes").font(.subheadline.weight(.semibold)).foregroundStyle(Theme.ink)
            }
            if let newest = model.newestFirmwareInBloom, newest > BuildInfo.latestFirmware {
                HStack {
                    Text("Newest in the bloom").font(.subheadline).foregroundStyle(Theme.inkDim)
                    Spacer()
                    Text(newest.text).font(.subheadline.weight(.semibold)).foregroundStyle(Theme.magenta)
                }
            }
            Divider().overlay(Color.white.opacity(0.15))
            Text(model.firmwareSummary).font(.caption).foregroundStyle(Theme.inkDim).fixedSize(horizontal: false, vertical: true)
            let mismatched = model.jellies.filter { !$0.missingModes.isEmpty }
            ForEach(mismatched) { j in
                Text("Jelly \(j.id) lacks \(modeList(j.missingModes)).").font(.caption).foregroundStyle(Theme.amber).fixedSize(horizontal: false, vertical: true)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .glassCard(padding: 12)
    }

    private func modeList(_ modes: [JellyMode]) -> String {
        let names = modes.prefix(4).map(\.name)
        return names.joined(separator: ", ") + (modes.count > 4 ? String(localized: " and \(modes.count - 4) more") : "")
    }

    // MARK: rows

    private func rosterRow(_ e: RosterEntry, isSelf: Bool = false) -> some View {
        let stale = !isSelf && Date().timeIntervalSince(e.lastSeen) > 90
        return HStack(spacing: 14) {
            Circle()
                .fill(e.slot >= 0 ? Theme.color(hue: JellyPalette.hue(forSlot: e.slot), saturation: 0.85, value: 0.95) : Color.white.opacity(0.15))
                .frame(width: 34, height: 34)
                .shadow(color: e.slot >= 0 ? Theme.color(hue: JellyPalette.hue(forSlot: e.slot)).opacity(0.6) : .clear, radius: 8)
                .overlay(Text(e.slot >= 0 ? "\(e.slot)" : "?").font(.caption.weight(.bold)).foregroundStyle(.black.opacity(0.7)))
            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 6) {
                    (isSelf ? Text("This phone") : (e.role == .app ? Text("Phone \(e.id)") : Text("Jelly \(e.id)"))).font(.headline).foregroundStyle(Theme.ink)
                    chip(roleName(e.role), tint: e.role == .ap ? Theme.magenta.opacity(0.35) : .white.opacity(0.1))
                }
                HStack(spacing: 6) {
                    versionChip(e, isSelf: isSelf)
                    if let n = e.modeCount { Text("\(n) modes").font(.caption2).foregroundStyle(Theme.inkDim) }
                }
                detailLine(e, isSelf: isSelf, stale: stale)
                    .font(.caption).foregroundStyle(Theme.inkDim)
            }
            Spacer()
        }
        .opacity(stale ? 0.6 : 1)
        .glassCard(padding: 12)
    }

    @ViewBuilder
    private func detailLine(_ e: RosterEntry, isSelf: Bool, stale: Bool) -> some View {
        if isSelf {
            if e.slot >= 0 { Text("virtual jelly, colour slot \(e.slot)") } else { Text("virtual jelly, waiting for a colour slot") }
        } else {
            let heard = stale ? String(localized: "last heard ") : ""
            HStack(spacing: 4) {
                // Every jelly serves its web page: its address opens it in Safari.
                if !e.ip.isEmpty, e.role != .app, let url = URL(string: "http://\(e.ip)/") {
                    Link(e.ip, destination: url).underline(true, pattern: .dot).foregroundStyle(Theme.cyan)
                } else {
                    Text(verbatim: e.ip.isEmpty ? String(localized: "address unknown") : e.ip)
                }
                Text(verbatim: "·  \(heard)\(e.lastSeen.formatted(date: .omitted, time: .standard))")
            }
        }
    }

    private func versionChip(_ e: RosterEntry, isSelf: Bool) -> some View {
        let status = e.firmwareStatus
        let tint: Color
        switch status {
        case .latest: tint = Theme.cyan.opacity(0.3)
        case .older: tint = Theme.amber.opacity(0.35)
        case .newer: tint = Theme.magenta.opacity(0.35)
        case .unknown: tint = .white.opacity(0.1)
        }
        let text: String
        if isSelf { text = BuildInfo.appVersion }
        else if let v = e.firmwareVersion { text = status == .latest ? v.text : "\(v.text) · \(status.label)" }
        else { text = e.role == .app ? String(localized: "app, no version") : String(localized: "no version · before \(BuildInfo.latestFirmware.text)") }
        return chip(text, tint: tint)
    }

    private func chip(_ text: String, tint: Color) -> some View {
        Text(text).font(.caption2.weight(.semibold)).padding(.horizontal, 6).padding(.vertical, 2)
            .background(tint, in: Capsule()).foregroundStyle(Theme.ink)
    }

    private func roleName(_ r: RosterEntry.Role) -> String {
        switch r { case .ap: return String(localized: "runs the network"); case .station: return String(localized: "jelly"); case .app: return String(localized: "app") }
    }
}
