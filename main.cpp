#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
// #include "nlohmann/json.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <functional>
#include <csignal>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

// For socket communication (Linux / MacOS)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// using json = nlohmann::json;
std::string get_json_value(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\":\"";
    size_t start_pos = json.find(search_key);
    if (start_pos == std::string::npos) {
        return "";
    }
    start_pos += search_key.length();

    size_t end_pos = start_pos;
    while ((end_pos = json.find('"', end_pos)) != std::string::npos) {
        if (end_pos > 0 && json.at(end_pos - 1) == '\\') {
            end_pos++;
        } else {
            break;
        }
    }

    if (end_pos == std::string::npos) {
        return "";
    }

    std::string raw_value = json.substr(start_pos, end_pos - start_pos);

    std::string final_value;
    final_value.reserve(raw_value.length());
    for (size_t i = 0; i < raw_value.length(); ++i) {
        if (raw_value[i] == '\\' && i + 1 < raw_value.length()) {
            char next_char = raw_value[i+1];
            if (next_char == '"' || next_char == '\\') {
                final_value += next_char;
                i++;
            } else {
                final_value += '\\';
            }
        } else {
            final_value += raw_value[i];
        }
    }
    
    return final_value;
}


struct MusicInfo {
    std::string artist;
    std::string track;
    std::string album;
    std::string source;
    std::string status;
};


std::mutex music_mutex;
MusicInfo current_music_info;

const uint64_t APPLICATION_ID = 1400023129048219740; // You can modify this to your own app on Discord Dev

std::atomic<bool> running = true;

// Signal handler to stop the application
void signalHandler(int signum) {
  running.store(false);
}

void listen_for_music_data() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    while (running) {
        // Create socket
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << u8"\n\u26A0 Socket creation error \n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(9999); // Port matching the Swift server

        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            std::cerr << u8"\n\u26A0 Invalid address/ Address not supported \n";
            close(sock);
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        std::cout << "Trying to connect to Swift data server..." << std::endl;
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << u8"\n\u26A0 Connection Failed. Is the NowPlayerServer running?\n";
            close(sock);
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait before retrying
            continue;
        }
        
        std::cout << u8"\u2705 Connected to NowPlayingServer. Waiting for song info..." << std::endl;

        // Keep reading from the connection
        while (running) {
            int valread = recv(sock, buffer, 1024, 0);
            if (valread > 0) {
                buffer[valread] = '\0'; // Null-terminate the received data
                std::string received_json(buffer);
                std::cout << "Received data: " << received_json << std::endl;

                std::lock_guard<std::mutex> lock(music_mutex);
                current_music_info.status = get_json_value(received_json, "status");
                current_music_info.artist = get_json_value(received_json, "artist");
                current_music_info.track = get_json_value(received_json, "track");
                current_music_info.album = get_json_value(received_json, "album");
                current_music_info.source = get_json_value(received_json, "source");

            } else {
                std::cerr << u8"\u26A0 Connection to Swift server lost. Reconnecting..." << std::endl;
                break;
            }
        }
        close(sock);
    }
}

std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }
    return escaped.str();
}

int main() {
    std::signal(SIGINT, signalHandler);
    
    // Start the network listener thread
    std::thread network_thread(listen_for_music_data);
    network_thread.detach(); // Let it run in the background

    std::cout << "🚀 Initializing Discord SDK...\n";

    // Create our Discord Client
    auto client = std::make_shared<discordpp::Client>();

    client->AddLogCallback([](auto message, auto severity) {
        std::cout << "[" << EnumToString(severity) << "] " << message << std::endl;
    }, discordpp::LoggingSeverity::Info);

    client->SetStatusChangedCallback([client](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
        std::cout << u8"\U0001F501 Status changed: " << discordpp::Client::StatusToString(status) << std::endl;
        if (status == discordpp::Client::Status::Ready) {
            std::cout << u8"\u2705 Client is ready! You can now call SDK functions.\n" << std::endl ;
        } else if (error != discordpp::Client::Error::None) {
            std::cerr << u8"\u26A0 Connection Error: " << discordpp::Client::ErrorToString(error) << " - Details: " << errorDetail << std::endl;
        }
    });

    // Start of Auth
    auto codeVerifier = client->CreateAuthorizationCodeVerifier();
    discordpp::AuthorizationArgs args{};
    args.SetClientId(APPLICATION_ID);
    args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
    args.SetCodeChallenge(codeVerifier.Challenge());

    client->Authorize(args, [client, codeVerifier](auto result, auto code, auto redirectUri) {
        if (!result.Successful()) {
            std::cerr << u8"\u26A0 Authentication Error: " << result.Error() << std::endl;
            return;
        }
        client->GetToken(APPLICATION_ID, code, codeVerifier.Verifier(), redirectUri,
            [client](discordpp::ClientResult result, std::string accessToken, std::string refreshToken, discordpp::AuthorizationTokenType tokenType, int32_t expiresIn, std::string scope) {
                client->UpdateToken(discordpp::AuthorizationTokenType::Bearer, accessToken, [client](discordpp::ClientResult result) {
                    if(result.Successful()) {
                        std::cout << "🔑 Token updated, connecting to Discord...\n";
                        client->Connect();
                    }
                });
            });
    });
    // End of Auth

    MusicInfo last_sent_music_info;
    auto last_activity_time = std::chrono::steady_clock::now();
    const auto timeout_duration = std::chrono::minutes(5);
    
    while (running) {
        discordpp::RunCallbacks();
        MusicInfo info_to_send;
        {
            std::lock_guard<std::mutex> lock(music_mutex);
            info_to_send = current_music_info;
        }

        // Check if the song has changed since the last update
        if (    info_to_send.track  != last_sent_music_info.track 
            ||  info_to_send.artist != last_sent_music_info.artist 
            ||  info_to_send.album  != last_sent_music_info.album 
            ||  info_to_send.status != last_sent_music_info.status) {
            std::cout << "New change detected! Updating Discord Rich Presence..." << std::endl;

            discordpp::Activity activity;
            if (info_to_send.status == "Playing") {
                if (!info_to_send.track.empty()) {
                    activity.SetType(discordpp::ActivityTypes::Playing);
                    activity.SetDetails(info_to_send.track);
                    activity.SetState("by " + info_to_send.artist + " - " + info_to_send.album);
                    discordpp::ActivityButton button;
                    std::string search_term = info_to_send.track + " " + info_to_send.artist;
                    std::string search_url = "https://music.apple.com/us/search?term=" + url_encode(search_term);
                    button.SetLabel("Search on AM");
                    button.SetUrl(search_url);
                    activity.AddButton(button);
                }
            } else {
                std::cout << "🎵 Music paused. Setting status to Resting..." << std::endl;
                activity.SetState("Resting...");
            }
            client->UpdateRichPresence(activity, [](discordpp::ClientResult result) {
                if(result.Successful()) {
                    std::cout << u8"\u2705 Rich Presence updated successfully!\n";
                } else {
                    std::cerr << u8"\u26A0 Rich Presence update failed\n";
                }
            });
            // Update the last sent info
            last_sent_music_info = info_to_send;
        }
        // Timeout detection
        if (current_music_info.status == "Playing") {
            last_activity_time = std::chrono::steady_clock::now(); 
        }
        if (std::chrono::steady_clock::now() - last_activity_time > timeout_duration) {
            std::cout << u8"\u231B Inactivity timeout reached. Shutting down." << std::endl;
            running.store(false);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << u8"\U0001F6D1 Shutting down..." << std::endl;
    return 0;
}
