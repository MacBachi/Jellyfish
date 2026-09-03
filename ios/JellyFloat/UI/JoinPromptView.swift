import SwiftUI

/// Shown when the app comes to the foreground: switch to the jelly network?
struct JoinPromptView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var settings = model.settings
        VStack(spacing: 22) {
            Image("AppIcon").resizable().scaledToFit().frame(width: 96, height: 96).clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
                .shadow(color: Theme.cyan.opacity(0.4), radius: 20)
            VStack(spacing: 6) {
                Text("Join the jelly network?").font(.title2.weight(.semibold)).foregroundStyle(Theme.ink)
                Text("Your phone switches Wi-Fi to talk to the bloom. iOS will ask once more.").font(.subheadline).foregroundStyle(Theme.inkDim).multilineTextAlignment(.center)
            }
            Picker("Network", selection: $settings.useCustomNetwork) {
                Text(AppSettings.defaultSSID + "  default").tag(false)
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
            }
            Toggle("Ask every time", isOn: $settings.askBeforeJoining).font(.caption).foregroundStyle(Theme.inkDim)
        }
        .padding(24)
        .background(Theme.background)
    }
}
