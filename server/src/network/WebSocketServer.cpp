#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\network\WebSocketServer.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>

// SHA1 và Base64 implementation
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

std::string WebSocketServer::urlDecode(const std::string &str)
{
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '%')
        {
            if (i + 2 < str.size())
            {
                std::string hex = str.substr(i + 1, 2);
                char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
                result += ch;
                i += 2;
            }
            else
            {
                result += '%';
            }
        }
        else if (str[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += str[i];
        }
    }

    return result;
}

WebSocketServer::WebSocketServer(int port) : port(port), serverSock(-1), running(false)
{
    platform = Platform::createPlatform();
    cmdHandler = std::make_unique<CommandHandler>();
    logFile = "websocket_access.log";
    platform->initSockets();
}

WebSocketServer::~WebSocketServer()
{
    stop();
    platform->cleanupSockets();
}

bool WebSocketServer::start()
{
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == INVALID_SOCKET)
        return false;

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverSock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        platform->closeSocket(serverSock);
        return false;
    }

    if (listen(serverSock, 10) < 0)
    {
        platform->closeSocket(serverSock);
        return false;
    }

    running = true;
    return true;
}

void WebSocketServer::stop()
{
    running = false;

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto &[sock, client] : clients)
    {
        platform->closeSocket(sock);
    }
    clients.clear();

    if (serverSock != INVALID_SOCKET)
    {
        platform->closeSocket(serverSock);
        serverSock = -1;
    }
}

void WebSocketServer::run()
{
    std::cout << "WebSocket server started on ws://localhost:" << port << "\n";

    while (running)
    {
        sockaddr_in client{};
        socklen_t len = sizeof(client);
        SocketHandle clientSock = accept(serverSock, (sockaddr *)&client, &len);

        if (clientSock == INVALID_SOCKET)
            continue;

        // Handle in new thread
        std::thread([this, clientSock]()
                    {
                        char buffer[65536];

                        // Wait for WebSocket handshake
                        int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
                        if (bytes <= 0)
                        {
                            platform->closeSocket(clientSock);
                            return;
                        }

                        buffer[bytes] = '\0';
                        std::string request(buffer);

                        // Perform WebSocket handshake
                        if (!handleHandshake(clientSock, request))
                        {
                            platform->closeSocket(clientSock);
                            return;
                        }

                        // Create client entry
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            Client newClient;
                            newClient.socket = clientSock;
                            newClient.authenticated = false;
                            newClient.connectedAt = std::time(nullptr);
                            clients[clientSock] = newClient;
                        }

                        std::cout << "WebSocket client connected\n";

                        // Listen for messages
                        while (running)
                        {
                            bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
                            if (bytes <= 0)
                                break;

                            buffer[bytes] = '\0';
                            std::string frame(buffer, bytes);

                            std::string message = decodeFrame(frame);
                            if (!message.empty())
                            {
                                handleMessage(clientSock, message);
                            }
                        }

                        // Clean up
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            clients.erase(clientSock);
                        }
                        platform->closeSocket(clientSock);
                        std::cout << "WebSocket client disconnected\n"; })
            .detach();
    }
}

bool WebSocketServer::handleHandshake(SocketHandle clientSock, const std::string &request)
{
    // Extract WebSocket key
    size_t keyPos = request.find("Sec-WebSocket-Key: ");
    if (keyPos == std::string::npos)
        return false;

    keyPos += 19;
    size_t keyEnd = request.find("\r\n", keyPos);
    std::string clientKey = request.substr(keyPos, keyEnd - keyPos);

    // Generate accept key
    std::string acceptKey = generateAcceptKey(clientKey);

    // Build handshake response
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
             << "\r\n";

    std::string responseStr = response.str();
    send(clientSock, responseStr.c_str(), responseStr.size(), 0);

    return true;
}

std::string WebSocketServer::generateAcceptKey(const std::string &clientKey)
{
    std::string magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magicString;

    // SHA1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)combined.c_str(), combined.size(), hash);

    // Base64 encode
    return base64Encode(hash, SHA_DIGEST_LENGTH);
}

std::string WebSocketServer::base64Encode(const unsigned char *input, size_t length)
{
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

std::string WebSocketServer::sha1(const std::string &input)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)input.c_str(), input.size(), hash);
    return std::string((char *)hash, SHA_DIGEST_LENGTH);
}

std::string WebSocketServer::decodeFrame(const std::string &frame)
{
    if (frame.size() < 2)
        return "";

    unsigned char byte2 = frame[1];

    bool masked = (byte2 & 0x80) != 0;
    unsigned long long payloadLen = byte2 & 0x7F;

    size_t pos = 2;

    // Extended payload length
    if (payloadLen == 126)
    {
        if (frame.size() < pos + 2)
            return "";
        payloadLen = ((unsigned char)frame[pos] << 8) | (unsigned char)frame[pos + 1];
        pos += 2;
    }
    else if (payloadLen == 127)
    {
        if (frame.size() < pos + 8)
            return "";
        payloadLen = 0;
        for (int i = 0; i < 8; i++)
        {
            payloadLen = (payloadLen << 8) | (unsigned char)frame[pos + i];
        }
        pos += 8;
    }

    // Masking key
    unsigned char maskingKey[4] = {0};
    if (masked)
    {
        if (frame.size() < pos + 4)
            return "";
        for (int i = 0; i < 4; i++)
        {
            maskingKey[i] = frame[pos + i];
        }
        pos += 4;
    }

    // Payload
    if (frame.size() < pos + payloadLen)
        return "";

    std::string payload;
    for (unsigned long long i = 0; i < payloadLen; i++)
    {
        unsigned char byte = frame[pos + i];
        if (masked)
        {
            byte ^= maskingKey[i % 4];
        }
        payload += byte;
    }

    return payload;
}

std::string WebSocketServer::encodeFrame(const std::string &message)
{
    std::string frame;

    // Byte 1: FIN + opcode (text frame)
    frame += (char)0x81;

    // Byte 2: mask + payload length
    size_t len = message.size();
    if (len <= 125)
    {
        frame += (char)len;
    }
    else if (len <= 65535)
    {
        frame += (char)126;
        frame += (char)((len >> 8) & 0xFF);
        frame += (char)(len & 0xFF);
    }
    else
    {
        frame += (char)127;
        for (int i = 7; i >= 0; i--)
        {
            frame += (char)((len >> (i * 8)) & 0xFF);
        }
    }

    // Payload
    frame += message;

    return frame;
}

void WebSocketServer::handleMessage(SocketHandle clientSock, const std::string &message)
{
    std::lock_guard<std::mutex> lock(clientsMutex);

    if (clients.find(clientSock) == clients.end())
        return;

    Client &client = clients[clientSock];
    std::string response = processCommand(message, client);

    sendMessage(clientSock, response);
}

void WebSocketServer::sendMessage(SocketHandle clientSock, const std::string &message)
{
    std::string frame = encodeFrame(message);
    send(clientSock, frame.c_str(), frame.size(), 0);
}

std::string WebSocketServer::processCommand(const std::string &jsonMessage, Client &client)
{
    // Extract action type
    std::string type = extractJSONValue(jsonMessage, "type");

    if (type == "login")
    {
        std::string username = extractJSONValue(jsonMessage, "username");
        return handleLogin(username, client);
    }
    else if (type == "command")
    {
        if (!client.authenticated)
        {
            return buildJSON({{"success", "false"}, {"error", "Not authenticated"}});
        }
        std::string command = extractJSONValue(jsonMessage, "command");
        return handleCommandExecution(command, client);
    }
    else if (type == "processes")
    {
        if (!client.authenticated)
        {
            return buildJSON({{"success", "false"}, {"error", "Not authenticated"}});
        }
        return handleProcessList(client);
    }
    else if (type == "sysinfo")
    {
        if (!client.authenticated)
        {
            return buildJSON({{"success", "false"}, {"error", "Not authenticated"}});
        }
        return handleSystemInfo(client);
    }
    else if (type == "history")
    {
        if (!client.authenticated)
        {
            return buildJSON({{"success", "false"}, {"error", "Not authenticated"}});
        }
        return handleHistory(client);
    }
    else if (type == "logs")
    {
        if (!client.authenticated)
        {
            return buildJSON({{"success", "false"}, {"error", "Not authenticated"}});
        }
        return handleLogs();
    }

    return buildJSON({{"success", "false"}, {"error", "Unknown command type"}});
}

std::string WebSocketServer::handleLogin(const std::string &username, Client &client)
{
    client.username = username.empty() ? "anonymous" : username;
    client.sessionId = generateSessionId();
    client.authenticated = true;

    auto sysInfo = platform->getSystemInfo();

    logAction(client, "LOGIN", "User logged in");

    std::ostringstream json;
    json << "{\"type\":\"login\","
         << "\"success\":true,"
         << "\"sessionId\":\"" << client.sessionId << "\","
         << "\"serverInfo\":{"
         << "\"os\":\"" << sysInfo.osName << "\","
         << "\"hostname\":\"" << sysInfo.hostname << "\","
         << "\"cpuCores\":" << sysInfo.cpuCores << ","
         << "\"totalMemory\":" << sysInfo.totalMemory
         << "}}";

    return json.str();
}

std::string WebSocketServer::handleCommandExecution(const std::string &command, Client &client)
{
    std::string result = cmdHandler->execute(command);

    logAction(client, "COMMAND", command);

    std::ostringstream json;
    json << "{\"type\":\"command\","
         << "\"success\":true,"
         << "\"result\":\"" << escapeJSON(result) << "\"}";

    return json.str();
}

std::string WebSocketServer::handleProcessList(Client &client)
{
    (void)client;
    auto processes = platform->listProcesses();

    std::ostringstream json;
    json << "{\"type\":\"processes\","
         << "\"success\":true,"
         << "\"processes\":[";

    bool first = true;
    int count = 0;
    for (const auto &proc : processes)
    {
        if (proc.name.empty() || proc.name == "<unknown>")
            continue;
        if (count++ >= 100)
            break;

        if (!first)
            json << ",";
        first = false;

        json << "{\"pid\":" << proc.pid << ","
             << "\"name\":\"" << escapeJSON(proc.name) << "\","
             << "\"memory\":" << proc.memoryUsage << "}";
    }

    json << "]}";
    return json.str();
}

std::string WebSocketServer::handleSystemInfo(Client &client)
{
    (void)client;
    auto info = platform->getSystemInfo();

    std::ostringstream json;
    json << "{\"type\":\"sysinfo\","
         << "\"success\":true,"
         << "\"system\":{"
         << "\"os\":\"" << info.osName << "\","
         << "\"arch\":\"" << info.architecture << "\","
         << "\"hostname\":\"" << info.hostname << "\","
         << "\"cpuCores\":" << info.cpuCores << ","
         << "\"totalMemory\":" << info.totalMemory << ","
         << "\"availableMemory\":" << info.availableMemory << "}}";

    return json.str();
}

std::string WebSocketServer::handleHistory(Client &client)
{
    (void)client;
    return buildJSON({{"type", "history"}, {"success", "true"}, {"history", "[]"}});
}

std::string WebSocketServer::handleLogs()
{
    std::ifstream file(logFile);
    std::string logs;

    if (file)
    {
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(file, line))
        {
            lines.push_back(line);
        }

        size_t start = lines.size() > 50 ? lines.size() - 50 : 0;
        for (size_t i = start; i < lines.size(); i++)
        {
            logs += lines[i] + "\\n";
        }
    }

    std::ostringstream json;
    json << "{\"type\":\"logs\","
         << "\"success\":true,"
         << "\"logs\":\"" << escapeJSON(logs) << "\"}";

    return json.str();
}

void WebSocketServer::logAction(const Client &client, const std::string &action, const std::string &details)
{
    std::lock_guard<std::mutex> lock(logMutex);

    std::ofstream file(logFile, std::ios::app);
    if (file)
    {
        file << getCurrentTimestamp() << " | "
             << "User: " << client.username << " | "
             << "Session: " << client.sessionId << " | "
             << "Action: " << action << " | "
             << "Details: " << details << "\n";
    }
}

std::string WebSocketServer::getCurrentTimestamp()
{
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string WebSocketServer::generateSessionId()
{
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string id;
    for (int i = 0; i < 32; i++)
    {
        id += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return id;
}

std::string WebSocketServer::buildJSON(const std::map<std::string, std::string> &data)
{
    std::ostringstream json;
    json << "{";
    bool first = true;
    for (const auto &[key, value] : data)
    {
        if (!first)
            json << ",";
        first = false;
        json << "\"" << key << "\":\"" << value << "\"";
    }
    json << "}";
    return json.str();
}

std::string WebSocketServer::extractJSONValue(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos)
        return "";

    pos += searchKey.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos)
        return "";

    return json.substr(pos, end - pos);
}

std::string WebSocketServer::escapeJSON(const std::string &str)
{
    std::string result;
    for (char c : str)
    {
        switch (c)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
        }
    }
    return result;
}