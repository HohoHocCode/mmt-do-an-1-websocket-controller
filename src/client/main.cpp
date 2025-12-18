#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <cstdio>

#include "network/ws_client.hpp"
#include "utils/json.hpp"
#include "utils/base64.hpp"

void print_menu() {
    std::cout << "\n========================\n";
    std::cout << " WebSocket Client Menu\n";
    std::cout << "========================\n";
    std::cout << "1. List processes\n";
    std::cout << "2. Kill process\n";
    std::cout << "3. Start process\n";
    std::cout << "4. Capture screen\n";
    std::cout << "5. Ping server\n";
    std::cout << "6. Capture camera\n"; 
    std::cout << "7. Record webcam 10s\n";
    std::cout << "8. Screen stream (5s @ 5fps)\n";
    std::cout << "0. Exit\n";
    std::cout << "------------------------\n";
    std::cout << "Enter your choice: ";
}

int main()
{
    WsClient client;

    // Handle error
    client.set_error_handler([](const std::string& err){
        std::cerr << "[ERROR] " << err << std::endl;
    });

    // Handle server message
    client.set_message_handler([](const std::string& raw){
        try {
            Json json = Json::parse(raw);

            // --- STREAM FRAMES ---
            if (json.contains("cmd") && json["cmd"] == "screen_stream" &&
                json.contains("image_base64"))
            {
                std::string b64 = json["image_base64"];
                std::vector<unsigned char> img = base64_decode(b64);

                int seq = 0;
                if (json.contains("seq") && json["seq"].is_number_integer())
                    seq = json["seq"];

                char namebuf[64];
                std::snprintf(namebuf, sizeof(namebuf), "stream_%05d.jpg", seq);
                std::string filename = namebuf;

                FILE* f = fopen(filename.c_str(), "wb");
                if (f) {
                    fwrite(img.data(), 1, img.size(), f);
                    fclose(f);

                    std::cout << "[CLIENT] Saved " << filename
                              << " (" << img.size() << " bytes)\n";
                } else {
                    std::cerr << "[CLIENT] Failed to save " << filename << "\n";
                }

                return;
            }

            // Screen or Camera image (single shot)
            if (json.contains("image_base64"))
            {
                std::string b64 = json["image_base64"];
                std::vector<unsigned char> img = base64_decode(b64);

                // --- distinguish screen vs camera ---
                std::string filename = "screenshot.jpg";

                if (json.contains("cmd") && json["cmd"] == "camera") {
                    filename = "camera.jpg";
                }

                FILE* f = fopen(filename.c_str(), "wb");
                fwrite(img.data(), 1, img.size(), f);
                fclose(f);

                std::cout << "[CLIENT] Saved " << filename 
                          << " (" << img.size() << " bytes)\n";

                return; // image handled, skip printing json
            }

            // Process list -> pretty print
            if (json.contains("data") && json["data"].is_array()) {
                std::cout << "[SERVER] Data:\n";
                for (auto& item : json["data"]) {
                    if (item.is_string())
                        std::cout << "  " << item.get<std::string>() << "\n";
                    else
                        std::cout << "  " << item.dump() << "\n";
                }
                return;
            }

            // General JSON printing
            std::cout << "[SERVER] " << json.dump(4) << "\n";
        }
        catch (...) {
            std::cout << "[SERVER RAW] " << raw << "\n";
        }
    });

    // Connect
    std::cout << "[Client] Connecting...\n";
    client.connect("127.0.0.1", "9002");

    // Wait for handshake
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Menu loop
    while (true) {
        print_menu();
        int choice;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 0) break;

        switch (choice) {
        case 1:
            client.send(R"({"cmd":"process_list"})");
            break;

        case 2: {
            int pid;
            std::cout << "Enter PID to kill: ";
            std::cin >> pid;
            std::cin.ignore();

            Json j;
            j["cmd"] = "process_kill";
            j["pid"] = pid;
            client.send(j.dump());
            break;
        }

        case 3: {
            std::string path;
            std::cout << "Enter process path: ";
            std::getline(std::cin, path);

            Json j;
            j["cmd"] = "process_start";
            j["path"] = path;
            client.send(j.dump());
            break;
        }

        case 4: // Capture screen
            client.send(R"({"cmd":"screen"})");
            break;

        case 5:
            client.send(R"({"cmd":"ping"})");
            break;

        case 6: // Capture camera
            client.send(R"({"cmd":"camera"})");
            break;

        case 7: { // Record webcam 10s
            Json j;
            j["cmd"] = "camera_video";
            j["duration"] = 10;    // hard-code 10 giây
            client.send(j.dump());
            break;
        }

        case 8: { // Screen stream
            Json j;
            j["cmd"] = "screen_stream";
            j["duration"] = 5;     // 5s
            j["fps"] = 5;          // 5 FPS
            client.send(j.dump());
            break;
        }

        default:
            std::cout << "Invalid option!\n";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}
