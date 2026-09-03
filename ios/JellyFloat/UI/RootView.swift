import SwiftUI

struct RootView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var model = model
        TabView {
            HomeView().tabItem { Label("Bloom", systemImage: "sparkles") }
            SwarmView().tabItem { Label("Jellies", systemImage: "circle.hexagongrid") }
            SettingsView().tabItem { Label("Settings", systemImage: "gearshape") }
        }
        .tint(Theme.cyan)
        .sheet(isPresented: $model.showJoinPrompt) {
            JoinPromptView().presentationDetents([.medium, .large]).presentationDragIndicator(.visible)
        }
    }
}
