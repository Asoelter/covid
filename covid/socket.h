#ifndef SOCKET_H
#define SOCKET_H

#include <memory>
#include <string>
#include <vector>

class SocketImpl;
class Socket;

class Port
{
public:
    Port(const std::string& ipAddress, const std::string& portNumber);
    Port(const Port& rhs) = delete;
    Port(Port&& rhs) noexcept;
    ~Port();

    Port& operator=(const Port& rhs) = delete;
    Port& operator=(Port&& rhs) noexcept;

    std::string ipAddress() const noexcept;
    std::string portNumber() const noexcept;

private:
    friend Socket;
    std::unique_ptr<SocketImpl> listen() const;

private:
    std::string                 ipAddress_;
    std::string                 portNumber_;
    std::unique_ptr<SocketImpl> portListener_;
};

class Socket
{
public:
    static Socket listenOn(const Port& port);
    static Socket&& connectTo(const Port& port);

    Socket(const  Socket& rhs) = delete;
    Socket(Socket&& rhs);
    ~Socket();

    Socket& operator=(const Socket& rhs) = delete;
    Socket& operator=(Socket&& rhs);

    std::vector<char> receive() const;

private:
    Socket(std::unique_ptr<SocketImpl>&& socketImpl);

private:
    std::unique_ptr<SocketImpl> socket_;
};

#endif //SOCKET_H
