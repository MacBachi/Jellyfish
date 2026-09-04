import SwiftUI

struct SettingsView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        SettingsForm(model: model, settings: model.settings)
    }
}

private struct SettingsForm: View {
    @ObservedObject var model: AppModel
    @ObservedObject var settings: AppSettings

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Picker("Network", selection: $settings.useCustomNetwork) {
                        Text("Default  \(AppSettings.defaultSSID)").tag(false)
                        Text("Custom").tag(true)
                    }
                    if settings.useCustomNetwork {
                        TextField("Network name", text: $settings.customSSID).textInputAutocapitalization(.never).autocorrectionDisabled()
                        SecureField("Password", text: $settings.customPassword)
                        Toggle("Remember network name", isOn: $settings.rememberNetwork)
                        Toggle("Remember password", isOn: $settings.rememberPassword).disabled(!settings.rememberNetwork)
                    }
                } header: { Text("Jelly network") } footer: {
                    if settings.useCustomNetwork { Text("The name is kept on this phone; the password only if you say so, in the Keychain.") } else { Text("Every jelly with the default firmware opens this network.") }
                }

                Section {
                    Toggle("Join when the app opens", isOn: $settings.joinOnForeground)
                    Toggle("Ask first", isOn: $settings.askBeforeJoining).disabled(!settings.joinOnForeground)
                    Toggle("Stay on the jelly network in the background", isOn: $settings.stayConnectedInBackground)
                } header: { Text("Switching") } footer: {
                    Text("iOS shows its own dialog before joining. When \"stay\" is off, iOS returns to your usual Wi-Fi about 15 seconds after you leave the app.")
                }

                Section {
                    Button("Join now") { Task { await model.joinAndConnect() } }
                    Button("Connect without joining") { model.connectWithoutJoining() }
                    Button("Disconnect", role: .destructive) { model.stop() }
                    if let error = model.joinError { Text(error).font(.caption).foregroundStyle(.red) }
                } header: { Text("Connection · \(model.connection.label)") } footer: {
                    if !model.linkStatus.isEmpty { Text("Link: \(model.linkStatus)") }
                }

                Section {
                    Stepper("Ring LEDs: \(settings.virtualRingLEDs)", value: $settings.virtualRingLEDs, in: 12...144, step: 1)
                        .onChange(of: settings.virtualRingLEDs) { _ in model.rebuildEngine() }
                    Toggle("Demo bloom (no hardware)", isOn: $settings.demoMode)
                        .onChange(of: settings.demoMode) { on in if on { model.startDemo() } else { model.stop() } }
                } header: { Text("Virtual jelly") } footer: {
                    Text("This phone joins the bloom as a jelly of its own: it gets a colour slot and runs the same effects on the shared clock.")
                }

                Section {
                    LabeledContent("This phone's jelly id", value: settings.jellyID)
                    LabeledContent("App version", value: BuildInfo.appVersion)
                    LabeledContent("Build", value: "\(BuildInfo.build) · \(BuildInfo.gitRevision)")
                    LabeledContent("Newest firmware", value: BuildInfo.latestFirmware.text)
                    LabeledContent("Modes this app knows", value: "\(BuildInfo.modeCount)")
                    Link("JellyFloatOS on GitHub", destination: URL(string: "https://github.com/MacBachi/Jellyfish")!)
                } header: { Text("About") } footer: {
                    Text("App and firmware share one version number. The Jellies tab shows what each jelly in the bloom runs.")
                }
            }
            .scrollContentBackground(.hidden)
            .background(Theme.background)
            .navigationTitle("Settings")
        }
    }
}
