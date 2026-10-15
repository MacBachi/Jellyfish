import SwiftUI

struct RootView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        TabView {
            HomeView().tabItem { Label("Bloom", systemImage: "sparkles") }
            SwarmView().tabItem { Label("Jellies", systemImage: "circle.hexagongrid") }
            SettingsView().tabItem { Label("Settings", systemImage: "gearshape") }
        }
        .tint(Theme.cyan)
        .sheet(isPresented: $model.showJoinPrompt) {
            JoinPromptView().presentationDetents([.medium, .large]).presentationDragIndicator(.visible)
        }
        .confirmationDialog("Join the jelly network automatically?", isPresented: $model.showJoinDeclined, titleVisibility: .visible) {
            Button("Not for an hour") { model.pauseAutoJoin(hours: 1) }
            Button("Never automatically", role: .destructive) { model.disableAutoJoin() }
            Button("Try again now") { model.showJoinDeclined = false; Task { await model.joinAndConnect() } }
            Button("Only not now", role: .cancel) { model.showJoinDeclined = false }
        } message: {
            Text("You can switch it back on under Settings, Switching.")
        }
    }
}
