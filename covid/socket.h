#ifndef SOCKET_H
#define SOCKET_H

#include <memory>
#include <string>
#include <vector>

class SocketImpl;

class ServerSocket
{
public:
    ServerSocket();
    ServerSocket(const ServerSocket& rhs);
    //ServerSocket(ServerSocket&&) = default;
    ~ServerSocket();

    ServerSocket& operator=(const ServerSocket& rhs);
    //ServerSocket& operator=(ServerSocket&&) = default;

    void listen(const std::string& ipAddress, const std::string& portNumber);
    std::vector<char> receive();

private:
    std::unique_ptr<SocketImpl> impl_;
    std::unique_ptr<SocketImpl> client_;
};

class ClientSocket
{
public:
    ClientSocket() = default;
    ~ClientSocket();

private:
    //std::unique_ptr<SocketImpl> impl_;
};


#endif //SOCKET_H
