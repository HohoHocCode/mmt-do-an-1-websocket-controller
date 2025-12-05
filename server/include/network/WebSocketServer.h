#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Platform.h"
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\CommandHandler.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <queue>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fstream>

// Simple WebSocket implementation
class WebSocketServer
{
private:
    struct Client
    {
        SocketHandle socket;
        std::string sessionId;
        std::string username;
        bool authenticated;
        std::time_t connectedAt;
        std::queue<std::string> messageQueue;
    };

    int port;
    SocketHandle serverSock;
    bool running;
    std::unique_ptr<Platform::IPlatform> platform;
    std::unique_ptr<CommandHandler> cmdHandler;

    std::map<SocketHandle, Client> clients;
    std::mutex clientsMutex;

    std::string logFile;
    std::mutex logMutex;

public:
    std::string urlDecode(const std::string &str);
    WebSocketServer(int port = 8080);
    ~WebSocketServer();

    bool start();
    void stop();
    void run();

private:
    // WebSocket handshake
    bool handleHandshake(SocketHandle clientSock, const std::string &request);
    std::string generateAcceptKey(const std::string &clientKey);

    // WebSocket frame handling
    std::string decodeFrame(const std::string &frame);
    std::string encodeFrame(const std::string &message);

    // Message handling
    void handleMessage(SocketHandle clientSock, const std::string &message);
    void sendMessage(SocketHandle clientSock, const std::string &message);
    void broadcast(const std::string &message);

    // Command processing
    std::string processCommand(const std::string &jsonMessage, Client &client);
    std::string handleLogin(const std::string &username, Client &client);
    std::string handleCommandExecution(const std::string &command, Client &client);
    std::string handleProcessList(Client &client);
    std::string handleSystemInfo(Client &client);
    std::string handleHistory(Client &client);
    std::string handleLogs();

    // JSON utilities (simple implementation)
    std::string buildJSON(const std::map<std::string, std::string> &data);
    std::string extractJSONValue(const std::string &json, const std::string &key);
    std::string escapeJSON(const std::string &str);

    // Logging
    void logAction(const Client &client, const std::string &action, const std::string &details);
    std::string getCurrentTimestamp();
    std::string generateSessionId();

    // Base64 encoding for WebSocket handshake
    std::string base64Encode(const unsigned char *input, size_t length);

    // SHA1 hash for WebSocket handshake
    std::string sha1(const std::string &input);
};

#endif // WEBSOCKET_SERVER_H