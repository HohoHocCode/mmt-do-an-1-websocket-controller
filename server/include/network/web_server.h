#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Network.h"
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\CommandHandler.h"
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Platform.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <iostream> // Added for std::cout

// Simple HTTP server for web interface
class WebServer
{
private:
    int port;
    SocketHandle serverSock;
    bool running;
    std::unique_ptr<Platform::IPlatform> platform;
    std::unique_ptr<CommandHandler> cmdHandler;
    std::mutex logMutex;
    std::string logFile;

    struct Session
    {
        std::string sessionId;
        std::string username;
        std::time_t createdAt;
        std::time_t lastActivity;
        std::vector<std::string> commandHistory;
    };

    std::map<std::string, Session> sessions;
    std::mutex sessionMutex;

public:
    WebServer(int port = 8080);
    ~WebServer();

    bool start();
    void stop();
    void run();

private:
    // HTTP handling
    std::string handleRequest(const std::string &request, const std::string &clientAddr);
    std::string parseHTTPMethod(const std::string &request);
    std::string parseHTTPPath(const std::string &request);
    std::string parseHTTPBody(const std::string &request);
    std::map<std::string, std::string> parseQueryString(const std::string &query);
    std::map<std::string, std::string> parseHeaders(const std::string &request);

    // Response builders
    std::string buildHTTPResponse(int code, const std::string &contentType,
                                  const std::string &body);
    std::string buildJSONResponse(const std::string &json);
    std::string buildErrorResponse(const std::string &error);

    // API endpoints
    std::string handleAPI(const std::string &path, const std::string &method,
                          const std::string &body, const std::string &clientAddr);
    std::string handleLogin(const std::string &body);
    std::string handleCommand(const std::string &body, const std::string &sessionId);
    std::string handleProcessList(const std::string &sessionId);
    std::string handleSystemInfo(const std::string &sessionId);
    std::string handleHistory(const std::string &sessionId);
    std::string handleLogs();

    // Session management
    std::string createSession(const std::string &username);
    bool validateSession(const std::string &sessionId);
    void updateSessionActivity(const std::string &sessionId);
    void logCommand(const std::string &sessionId, const std::string &username,
                    const std::string &command, const std::string &result);

    // Utilities
    std::string generateSessionId();
    std::string getCurrentTimestamp();
    std::string urlDecode(const std::string &str);
    std::string escapeJSON(const std::string &str);

    // *** ADDED DECLARATION ***
    std::string getIndexHTML();
};

#endif // WEB_SERVER_H