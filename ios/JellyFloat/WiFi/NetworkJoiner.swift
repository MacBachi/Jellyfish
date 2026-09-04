import Foundation
import NetworkExtension

enum JoinOutcome: Equatable {
    case joined
    case alreadyConnected
    case denied
    case unavailable(String)
    case failed(String)
}

/// Joins the jelly Wi-Fi through iOS' hotspot configuration API. iOS shows its own dialog
/// and, with joinOnce, leaves the network again once the app has been in the background
/// for 15 seconds.
enum NetworkJoiner {
    static func join(ssid: String, password: String, joinOnce: Bool) async -> JoinOutcome {
        #if targetEnvironment(simulator)
        return .unavailable(String(localized: "Joining Wi-Fi is not available in the simulator."))
        #else
        let config = password.isEmpty
            ? NEHotspotConfiguration(ssid: ssid)
            : NEHotspotConfiguration(ssid: ssid, passphrase: password, isWEP: false)
        config.joinOnce = joinOnce
        do {
            try await NEHotspotConfigurationManager.shared.apply(config)
            return .joined
        } catch let error as NSError where error.domain == NEHotspotConfigurationErrorDomain {
            switch NEHotspotConfigurationError(rawValue: error.code) {
            case .alreadyAssociated: return .alreadyConnected
            case .userDenied: return .denied
            case .invalidSSID: return .failed(String(localized: "iOS rejected the network name. Try an ASCII name via the firmware's JELL_WIFI_SSID option."))
            case .invalidWPAPassphrase: return .failed(String(localized: "The password must be 8 to 63 characters."))
            case .applicationIsNotInForeground: return .failed(String(localized: "The app must be in the foreground to join."))
            default: return .failed(error.localizedDescription)
            }
        } catch {
            return .failed(error.localizedDescription)
        }
        #endif
    }

    static func forget(ssid: String) {
        #if !targetEnvironment(simulator)
        NEHotspotConfigurationManager.shared.removeConfiguration(forSSID: ssid)
        #endif
    }
}
