import SwiftUI

@main
struct JellyFloatApp: App {
    @StateObject private var model = AppModel()
    @StateObject private var updates = FirmwareUpdates()
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(model)
                .environmentObject(updates)
                .preferredColorScheme(.dark)
        }
        .onChange(of: scenePhase) { phase in model.scenePhaseChanged(phase) }
    }
}
