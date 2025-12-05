#ifndef NETWORK_H
#define NETWORK_H

#include "Platform.h"
#include <string>
#include <memory>
#include <functional>

namespace Network
{
    // Defines the communication protocol (TCP or UDP)
    enum class Protocol
    {
        TCP,
        UDP
    };

    // Defines the type of message being sent
    enum class MessageType
    {
        COMMAND,        // A command from client to server (e.g., "list")
        RESPONSE,       // A response from server to client (e.g., process list)
        HEARTBEAT,      // A check to see if the connection is alive
        FILE_TRANSFER,  // (Not yet implemented)
        SCREEN_CAPTURE, // (Not yet implemented)
        ERR             // An error occurred
    };

    /**
     * @struct Message
     * @brief Defines the data structure for all network communication.
     *
     * All data sent between client and server is encapsulated in this struct,
     * which is then serialized into a string.
     */
    struct Message
    {
        MessageType type;    // What kind of message this is
        std::string command; // The command name (e.g., "list", "kill")
        std::string payload; // The arguments for a command or the data for a response
        size_t payloadSize;  // The size of the payload (used for serialization)

        /**
         * @brief Serializes the Message struct into a simple string format for sending.
         * @return A string representation of the message.
         */
        std::string serialize() const;

        /**
         * @brief Deserializes a string back into a Message struct.
         * @param data The string received from the network.
         * @return A populated Message struct.
         */
        static Message deserialize(const std::string &data);
    };

    /**
     * @class IConnection
     * @brief Abstract interface for a client-side network connection.
     */
    class IConnection
    {
    public:
        virtual ~IConnection() = default;
        virtual bool connect(const std::string &host, int port) = 0;
        virtual bool send(const Message &msg) = 0;
        virtual Message receive() = 0;
        virtual bool isConnected() const = 0;
        virtual void disconnect() = 0;
    };

    /**
     * @class IServer
     * @brief Abstract interface for a server-side network listener.
     */
    class IServer
    {
    public:
        virtual ~IServer() = default;
        virtual bool start(int port) = 0;
        virtual void stop() = 0;
        virtual bool waitForClient() = 0;
        virtual bool send(const Message &msg) = 0;
        virtual Message receive() = 0;
        virtual bool isRunning() const = 0;
        virtual std::string getClientInfo() const = 0;
    };

    // --- TCP Implementation ---
    // TCP Connection Implementation
    class TcpConnection : public IConnection
    {
        SocketHandle sock;                             // The client's socket
        bool connected;                                // Connection status flag
        std::unique_ptr<Platform::IPlatform> platform; // Platform helper (for init/close)

    public:
        TcpConnection();
        ~TcpConnection();
        bool connect(const std::string &host, int port) override;
        bool send(const Message &msg) override;
        Message receive() override;
        bool isConnected() const override { return connected; }
        void disconnect() override;
    };

    // TCP Server Implementation
    class TcpServer : public IServer
    {
        SocketHandle serverSock = INVALID_SOCKET; // The server's listening socket
        SocketHandle clientSock = INVALID_SOCKET; // The socket for the currently connected client
        bool running;                             // Server status flag
        std::string clientAddr;                   // String representation of the client's IP/port
        std::unique_ptr<Platform::IPlatform> platform;

    public:
        TcpServer();
        ~TcpServer();
        bool start(int port) override;
        void stop() override;
        bool waitForClient() override;
        bool send(const Message &msg) override;
        Message receive() override;
        bool isRunning() const override { return running; }
        std::string getClientInfo() const override { return clientAddr; }
    };

    // UDP Connection Implementation
    class UdpConnection : public IConnection
    {
        SocketHandle sock;
        sockaddr_in serverAddr;
        bool connected;
        std::unique_ptr<Platform::IPlatform> platform;

    public:
        UdpConnection();
        ~UdpConnection();
        bool connect(const std::string &host, int port) override;
        bool send(const Message &msg) override;
        Message receive() override;
        bool isConnected() const override { return connected; }
        void disconnect() override;
    };

    // UDP Server Implementation
    class UdpServer : public IServer
    {
        SocketHandle sock = INVALID_SOCKET; // The server's listening socket
        sockaddr_in clientAddr;             // Struct to hold the last client's address info
        socklen_t clientLen;                // Size of the clientAddr struct
        bool running;
        std::unique_ptr<Platform::IPlatform> platform;

    public:
        UdpServer();
        ~UdpServer();
        bool start(int port) override;
        void stop() override;
        bool waitForClient() override;
        bool send(const Message &msg) override;
        Message receive() override;
        bool isRunning() const override { return running; }
        std::string getClientInfo() const override;
    };

    // Factory functions
    std::unique_ptr<IConnection> createConnection(Protocol proto);
    std::unique_ptr<IServer> createServer(Protocol proto);
}

#endif // NETWORK_H