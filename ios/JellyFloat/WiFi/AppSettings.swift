import Foundation
import Observation
import Security

/// User settings. Everything lives in UserDefaults except the custom network password, which
/// goes to the Keychain and only if the user opted in.
@Observable
final class AppSettings {
    static let defaultSSID = "🪼"
    static let defaultPassword = "FroschUndMaus"

    private let defaults = UserDefaults.standard

    /// Use the default jelly network or a custom one.
    var useCustomNetwork: Bool { didSet { defaults.set(useCustomNetwork, forKey: "useCustomNetwork") } }
    var customSSID: String { didSet { if rememberNetwork { defaults.set(customSSID, forKey: "customSSID") } } }
    /// In memory only unless rememberPassword is on.
    var customPassword: String { didSet { if rememberPassword { Keychain.set(customPassword, account: "customPassword") } } }
    /// Remember the custom SSID (never the password unless rememberPassword). Default on.
    var rememberNetwork: Bool {
        didSet {
            defaults.set(rememberNetwork, forKey: "rememberNetwork")
            if rememberNetwork { defaults.set(customSSID, forKey: "customSSID") } else { defaults.removeObject(forKey: "customSSID"); rememberPassword = false }
        }
    }
    /// Keep the custom password in the Keychain. Default off.
    var rememberPassword: Bool {
        didSet {
            defaults.set(rememberPassword, forKey: "rememberPassword")
            if rememberPassword { Keychain.set(customPassword, account: "customPassword") } else { Keychain.delete(account: "customPassword") }
        }
    }
    /// Join the jelly network whenever the app comes to the foreground. Default on.
    var joinOnForeground: Bool { didSet { defaults.set(joinOnForeground, forKey: "joinOnForeground") } }
    /// Ask before joining. Default on.
    var askBeforeJoining: Bool { didSet { defaults.set(askBeforeJoining, forKey: "askBeforeJoining") } }
    /// Keep the jelly network when the app goes to the background. Default off: iOS then
    /// drops it after 15 s and returns to the previous network by itself.
    var stayConnectedInBackground: Bool { didSet { defaults.set(stayConnectedInBackground, forKey: "stayConnectedInBackground") } }
    /// Talk to a pretend jelly inside the app instead of the network.
    var demoMode: Bool { didSet { defaults.set(demoMode, forKey: "demoMode") } }
    /// LEDs of the virtual jelly's ring.
    var virtualRingLEDs: Int { didSet { defaults.set(virtualRingLEDs, forKey: "virtualRingLEDs") } }

    /// This phone's four-hex-digit jelly id, created once.
    let jellyID: String

    var ssid: String { useCustomNetwork ? customSSID : AppSettings.defaultSSID }
    var password: String { useCustomNetwork ? customPassword : AppSettings.defaultPassword }

    init() {
        let d = UserDefaults.standard
        useCustomNetwork = d.bool(forKey: "useCustomNetwork")
        rememberNetwork = d.object(forKey: "rememberNetwork") as? Bool ?? true
        rememberPassword = d.bool(forKey: "rememberPassword")
        customSSID = d.string(forKey: "customSSID") ?? ""
        customPassword = d.bool(forKey: "rememberPassword") ? (Keychain.get(account: "customPassword") ?? "") : ""
        joinOnForeground = d.object(forKey: "joinOnForeground") as? Bool ?? true
        askBeforeJoining = d.object(forKey: "askBeforeJoining") as? Bool ?? true
        stayConnectedInBackground = d.bool(forKey: "stayConnectedInBackground")
        #if targetEnvironment(simulator)
        demoMode = d.object(forKey: "demoMode") as? Bool ?? true
        #else
        demoMode = d.bool(forKey: "demoMode")
        #endif
        virtualRingLEDs = d.object(forKey: "virtualRingLEDs") as? Int ?? 39
        if let id = d.string(forKey: "jellyID") {
            jellyID = id
        } else {
            let id = String(format: "%04x", Int.random(in: 0...0xffff))
            d.set(id, forKey: "jellyID")
            jellyID = id
        }
    }
}

/// Minimal Keychain wrapper for one generic password item per account.
enum Keychain {
    private static let service = "at.guggug.jellyfloat"

    static func set(_ value: String, account: String) {
        delete(account: account)
        let item: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecValueData as String: Data(value.utf8),
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
        ]
        SecItemAdd(item as CFDictionary, nil)
    }

    static func get(account: String) -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne,
        ]
        var out: AnyObject?
        guard SecItemCopyMatching(query as CFDictionary, &out) == errSecSuccess, let data = out as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    static func delete(account: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
        ]
        SecItemDelete(query as CFDictionary)
    }
}
