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
    }
}
