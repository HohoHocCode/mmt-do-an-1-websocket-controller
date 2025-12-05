
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Network.h"
#include <sstream>
#include <cstring>
#include <iostream>

namespace Network
{
    // Message serialization
    std::string Message::serialize() const
    {
        std::ostringstream oss;
        // Serialize the message using a pipe '|' as a delimiter
        // Format: [Type]|[Command]|[PayloadSize]|[Payload]
        oss << static_cast<int>(type) << "|"
            << command << "|"
            << payloadSize << "|"
            << payload;
        return oss.str();
    }

    Message Message::deserialize(const std::string &data)
    {
        Message msg;
        std::istringstream iss(data);
        std::string token;

        // Parse the pipe-delimited string
        std::getline(iss, token, '|');
        msg.type = static_cast<MessageType>(std::stoi(token));

        std::getline(iss, msg.command, '|');

        std::getline(iss, token, '|');
        msg.payloadSize = std::stoull(token);

        // Read the rest of the stream as the payload
        std::getline(iss, msg.payload);

        return msg;
    }

    // TCP Connection Implementation
    TcpConnection::TcpConnection() : sock(-1), connected(false)
    {
        platform = Platform::createPlatform();
        platform->initSockets();
    }

    TcpConnection::~TcpConnection()
    {
        disconnect();
        platform->cleanupSockets();
    }

    bool TcpConnection::connect(const std::string &host, int port)
    {
        // Create the socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET)
            return false;

        // Set up the server address structure
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port); // Convert port to network byte order

        // Convert IP address string to binary form
        if (inet_pton(AF_INET, host.c_str(), &server.sin_addr) <= 0)
        {
            platform->closeSocket(sock);
            return false;
        }

        // Connect to the server
        if (::connect(sock, (sockaddr *)&server, sizeof(server)) < 0)
        {
            platform->closeSocket(sock);
            return false;
        }

        connected = true;
        return true;
    }

    bool TcpConnection::send(const Message &msg)
    {
        if (!connected)
            return false;
        std::string data = msg.serialize();
        return ::send(sock, data.c_str(), data.size(), 0) > 0;
    }

    Message TcpConnection::receive()
    {
        char buffer[65536]; // Large buffer for receiving data
        // Block until data is received
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0)
        {
            // 0 bytes means graceful close, < 0 is an error
            connected = false;
            return Message{MessageType::ERR, "", "", 0};
        }
        buffer[bytes] = '\0'; // Null-terminate the received data
        return Message::deserialize(buffer);
    }

    void TcpConnection::disconnect()
    {
        if (connected)
        {
            platform->closeSocket(sock);
            connected = false;
        }
    }

    // TCP Server Implementation
    TcpServer::TcpServer() : serverSock(-1), clientSock(-1), running(false)
    {
        platform = Platform::createPlatform();
        platform->initSockets();
    }

    TcpServer::~TcpServer()
    {
        stop();
        platform->cleanupSockets();
    }

    bool TcpServer::start(int port)
    {
        serverSock = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSock == INVALID_SOCKET)
            return false;

        // Set socket option to reuse the address, avoids "Address already in use" error
        int opt = 1;
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

        // Set up the server address structure to bind to
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
        addr.sin_port = htons(port);

        // Bind the socket to the port
        if (bind(serverSock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            platform->closeSocket(serverSock);
            return false;
        }

        // Start listening for connections (backlog of 5)
        if (listen(serverSock, 5) < 0)
        {
            platform->closeSocket(serverSock);
            return false;
        }

        running = true;
        return true;
    }

    void TcpServer::stop()
    {
        running = false;
        if (clientSock != INVALID_SOCKET)
            platform->closeSocket(clientSock);
        if (serverSock != INVALID_SOCKET)
            platform->closeSocket(serverSock);
        clientSock = serverSock = INVALID_SOCKET;
    }

    bool TcpServer::waitForClient()
    {
        if (!running)
            return false;

        sockaddr_in client{};
        socklen_t len = sizeof(client);
        // Block and wait for a new client to connect
        clientSock = accept(serverSock, (sockaddr *)&client, &len);

        if (clientSock == INVALID_SOCKET)
            return false;

        // Convert client's IP to a string for logging
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client.sin_addr, clientIP, INET_ADDRSTRLEN);
        clientAddr = std::string(clientIP) + ":" + std::to_string(ntohs(client.sin_port));

        return true;
    }

    bool TcpServer::send(const Message &msg)
    {
        if (clientSock == INVALID_SOCKET)
            return false;
        std::string data = msg.serialize();
        return ::send(clientSock, data.c_str(), data.size(), 0) > 0;
    }

    Message TcpServer::receive()
    {
        char buffer[65536];
        int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0)
        {
            return Message{MessageType::ERR, "", "", 0};
        }
        buffer[bytes] = '\0';
        return Message::deserialize(buffer);
    }

    // UDP Connection Implementation
    UdpConnection::UdpConnection() : sock(-1), connected(false)
    {
        platform = Platform::createPlatform();
        platform->initSockets();
    }

    UdpConnection::~UdpConnection()
    {
        disconnect();
        platform->cleanupSockets();
    }

    bool UdpConnection::connect(const std::string &host, int port)
    {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET)
            return false;

        // For UDP, "connect" just means setting up the server address
        // structure for future sendto() calls. No actual connection is made.
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0)
        {
            platform->closeSocket(sock);
            return false;
        }

        connected = true;
        return true;
    }

    bool UdpConnection::send(const Message &msg)
    {
        if (!connected)
            return false;
        std::string data = msg.serialize();
        return sendto(sock, data.c_str(), data.size(), 0,
                      (sockaddr *)&serverAddr, sizeof(serverAddr)) > 0;
    }

    Message UdpConnection::receive()
    {
        char buffer[65536];
        socklen_t len = sizeof(serverAddr);
        int bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr *)&serverAddr, &len);
        // Block and wait for a datagram
        if (bytes <= 0)
        {
            connected = false;
            return Message{MessageType::ERR, "", "", 0};
        }
        buffer[bytes] = '\0';
        return Message::deserialize(buffer);
    }

    void UdpConnection::disconnect()
    {
        if (connected)
        {
            platform->closeSocket(sock);
            connected = false;
        }
    }

    // UDP Server Implementation
    UdpServer::UdpServer() : sock(-1), running(false)
    {
        platform = Platform::createPlatform();
        platform->initSockets();
        clientLen = sizeof(clientAddr);
    }

    UdpServer::~UdpServer()
    {
        stop();
        platform->cleanupSockets();
    }

    bool UdpServer::start(int port)
    {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET)
            return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            platform->closeSocket(sock);
            return false;
        }

        running = true;
        return true;
    }

    void UdpServer::stop()
    {
        running = false;
        if (sock != INVALID_SOCKET)
            platform->closeSocket(sock);
        sock = INVALID_SOCKET;
    }

    bool UdpServer::waitForClient()
    {
        return running; // UDP doesn't need explicit client acceptance
    }

    bool UdpServer::send(const Message &msg)
    {
        if (sock == INVALID_SOCKET)
            return false;
        std::string data = msg.serialize();
        return sendto(sock, data.c_str(), data.size(), 0,
                      (sockaddr *)&clientAddr, clientLen) > 0;
    }

    Message UdpServer::receive()
    {
        char buffer[65536];
        // Block and wait for a datagram, capture the client's address
        int bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr *)&clientAddr, &clientLen);
        if (bytes <= 0)
        {
            return Message{MessageType::ERR, "", "", 0};
        }
        buffer[bytes] = '\0';
        return Message::deserialize(buffer);
    }

    std::string UdpServer::getClientInfo() const
    {
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        return std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
    }

    // Factory functions
    std::unique_ptr<IConnection> createConnection(Protocol proto)
    {
        if (proto == Protocol::TCP)
            return std::make_unique<TcpConnection>();
        return std::make_unique<UdpConnection>();
    }

    std::unique_ptr<IServer> createServer(Protocol proto)
    {
        if (proto == Protocol::TCP)
            return std::make_unique<TcpServer>();
        return std::make_unique<UdpServer>();
    }

} // namespace Network
