import Foundation
import Network

/// Something that carries text lines to and from the jelly network.
protocol JellyTransport: AnyObject {
    /// Called on the main actor for every line received, with the local monotonic time (µs) of arrival.
    var onLine: ((String, Int64) -> Void)? { get set }
    var onStatus: ((String) -> Void)? { get set }
    func connect()
    func send(_ line: String)
    func disconnect()
}

/// UDP to the access point jelly. Replies come back on the same connection, so no
/// listener and no broadcast entitlement are needed.
final class UDPJellyLink: JellyTransport {
    var onLine: ((String, Int64) -> Void)?
    var onStatus: ((String) -> Void)?

    private let host: NWEndpoint.Host
    private let port: NWEndpoint.Port
    private let queue = DispatchQueue(label: "at.guggug.jellyfloat.udp")
    private var connection: NWConnection?

    init(host: String = "192.168.4.1", port: UInt16 = 4210) {
        self.host = NWEndpoint.Host(host)
        self.port = NWEndpoint.Port(rawValue: port)!
    }

    func connect() {
        disconnect()
        let params = NWParameters.udp
        #if !targetEnvironment(simulator)
        params.requiredInterfaceType = .wifi
        #endif
        let c = NWConnection(host: host, port: port, using: params)
        connection = c
        c.stateUpdateHandler = { [weak self] state in
            let text: String
            switch state {
            case .ready: text = "ready"
            case .waiting(let e): text = "waiting: \(e.localizedDescription)"
            case .failed(let e): text = "failed: \(e.localizedDescription)"
            case .cancelled: text = "cancelled"
            case .preparing: text = "preparing"
            case .setup: text = "setup"
            @unknown default: text = "unknown"
            }
            DispatchQueue.main.async { self?.onStatus?(text) }
            if case .ready = state { self?.receive(on: c) }
        }
        c.start(queue: queue)
    }

    func send(_ line: String) {
        guard let c = connection else { return }
        c.send(content: Data(line.utf8), completion: .contentProcessed { _ in })
    }

    func disconnect() {
        connection?.cancel()
        connection = nil
    }

    private func receive(on c: NWConnection) {
        c.receiveMessage { [weak self] data, _, _, error in
            guard let self else { return }
            let arrived = TimeSync.nowUs()
            if let data, let text = String(data: data, encoding: .utf8) {
                for line in text.split(separator: "\n") {
                    let l = String(line)
                    DispatchQueue.main.async { self.onLine?(l, arrived) }
                }
            }
            if error == nil, self.connection === c { self.receive(on: c) }
        }
    }
}
