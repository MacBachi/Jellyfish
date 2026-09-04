import SwiftUI

/// Shown when the app comes to the foreground: switch to the jelly network?
struct JoinPromptView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        JoinPromptForm(model: model, settings: model.settings)
    }
}

private struct JoinPromptForm: View {
    @ObservedObject var model: AppModel
    @ObservedObject var settings: AppSettings

    var body: some View {
        VStack(spacing: 22) {
            Text(verbatim: "🪼").font(.system(size: 64)).shadow(color: Theme.cyan.opacity(0.5), radius: 20)
            VStack(spacing: 6) {
                Text("Join the jelly network?").font(.title2.weight(.semibold)).foregroundStyle(Theme.ink)
                Text("Your phone switches Wi-Fi to talk to the bloom. iOS will ask once more.").font(.subheadline).foregroundStyle(Theme.inkDim).multilineTextAlignment(.center)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Picker("Network", selection: $settings.useCustomNetwork) {
                Text("\(AppSettings.defaultSSID)  default").tag(false)
                Text("Custom").tag(true)
            }
            .pickerStyle(.segmented)
            if settings.useCustomNetwork {
                VStack(spacing: 10) {
                    TextField("Network name", text: $settings.customSSID).textInputAutocapitalization(.never).autocorrectionDisabled().textFieldStyle(.roundedBorder)
                    SecureField("Password", text: $settings.customPassword).textFieldStyle(.roundedBorder)
                    Toggle("Remember network name", isOn: $settings.rememberNetwork)
                    Toggle("Remember password too", isOn: $settings.rememberPassword).disabled(!settings.rememberNetwork)
                }
                .font(.subheadline).foregroundStyle(Theme.ink)
            }
            VStack(spacing: 10) {
                Button { Task { await model.joinAndConnect() } } label: { Text("Join \(settings.ssid)").frame(maxWidth: .infinity) }
                    .buttonStyle(.borderedProminent).tint(Theme.cyan).foregroundStyle(.black)
                Button { model.connectWithoutJoining() } label: { Text("I'm already on it").frame(maxWidth: .infinity) }
                    .buttonStyle(.bordered).tint(Theme.cyan)
                Button("Not now") { model.showJoinPrompt = false }.foregroundStyle(Theme.inkDim)
                Button("No jellyfish yet? Try the demo bloom") { settings.demoMode = true; model.showJoinPrompt = false; model.startDemo() }
                    .font(.footnote).foregroundStyle(Theme.inkDim)
            }
            Toggle("Ask every time", isOn: $settings.askBeforeJoining).font(.caption).foregroundStyle(Theme.inkDim)
        }
        .padding(24)
        .background(Theme.background)
    }
}
