import Foundation
import Network

class NowPlayingServer {
    private var listener: NWListener?
    private var connections: [NWConnection] = []
    private let port: NWEndpoint.Port = 9999

    init() {
        setupListener()
        setupNotificationObservers()
    }

    private func setupListener() {
        do {
            listener = try NWListener(using: .tcp, on: port)
            listener?.stateUpdateHandler = { [weak self] state in
                switch state {
                case .ready:
                    print("🎧 Server ready and listening on port \(self?.port.rawValue ?? 0)")
                case .failed(let error):
                    print("❌ Server failed with error: \(error)")
                default:
                    break
                }
            }

            listener?.newConnectionHandler = { [weak self] newConnection in
                print("✅ Client connected.")
                self?.handleNewConnection(newConnection)
            }

            listener?.start(queue: .global(qos: .background))
        } catch {
            print("❌ Unable to create listener: \(error)")
        }
    }

    private func handleNewConnection(_ connection: NWConnection) {
        connections.append(connection)
        
        connection.stateUpdateHandler = { [weak self] state in
            switch state {
            case .cancelled:
                print("👋 Client disconnected.")
                self?.connections.removeAll { $0 === connection }
            case .failed(let error):
                print("❌ Connection failed: \(error)")
                self?.connections.removeAll { $0 === connection }
            default:
                break
            }
        }
        
        connection.start(queue: .global())
    }

    private func setupNotificationObservers() {
        let center = DistributedNotificationCenter.default()

        // Apple Music
        center.addObserver( self,
                            selector: #selector(handleMusicNotification(_:)),
                            name: NSNotification.Name("com.apple.iTunes.playerInfo"),
                            object: nil)
        // Spotify (testing)
        // center.addObserver( self,
        //                     selector: #selector(handleMusicNotification(_:)),
        //                     name: NSNotification.Name("com.spotify.client.PlaybackStateChanged"),
        //                     object: nil)
    }

    private func broadcast(_ metadata: [String: String]) {
        guard let json = try? JSONSerialization.data(withJSONObject: metadata, options: []) else {
            print("❌ Could not serialize metadata to JSON.")
            return
        }

        print("📢 Broadcasting: \(String(data: json, encoding: .utf8) ?? "Invalid JSON")")
        
        for connection in connections {
            connection.send(content: json, completion: .contentProcessed { error in
                if let error = error {
                    print("📮 Send failed for a client: \(error)")
                }
            })
        }
    }

    @objc func handleMusicNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let playerState = userInfo["Player State"] as? String 
        else {
            broadcast(["status": "Paused"])
            return
        }
        
        let source = notification.name.rawValue.contains("spotify") ? "Spotify" : "Apple Music"

        if playerState == "Playing" {
            let progress = (userInfo["Player Position"] as? Double) ?? (userInfo["Playback Position"] as? Double) ?? 0.0
            let durationMS = userInfo["Total Time"] as? Double ?? 0.0
            let duration = durationMS > 0 ? durationMS / 1000.0 : 0.0

            let metadata: [String: String] = [
                "status": "Playing",
                "source": source,
                "artist": userInfo["Artist"] as? String ?? "Unknown Artist",
                "album": userInfo["Album"] as? String ?? "Unknown Album",
                "track": userInfo["Name"] as? String ?? "Unknown Track",
                "progress": String(progress),
                "duration": String(duration)
            ]
            broadcast(metadata)
        } else {
            // For "Paused", "Stopped", etc.
            broadcast(["status": "Paused"])
        }
    }

    deinit {
        DistributedNotificationCenter.default().removeObserver(self)
        listener?.cancel()
    }
}

// --- Main Execution ---
let server = NowPlayingServer()
RunLoop.main.run()
