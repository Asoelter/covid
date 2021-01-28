#include "socket.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include "exception.h"

class SocketImpl
{
public:
    using Ptr = std::unique_ptr<SocketImpl>;

    SocketImpl();
    SocketImpl(const SocketImpl& rhs) = delete;
    SocketImpl(SocketImpl&& rhs) noexcept;
    ~SocketImpl();

    SocketImpl& operator=(const SocketImpl& rhs) noexcept = delete;
    SocketImpl& operator=(SocketImpl&& rhs) noexcept;

    Ptr waitForClient(const std::string& ipAddress, const std::string& portNumber);
    void connectTo(const std::string& ipAddress, const std::string& portNumber);

    void send(const std::vector<char>& message) const;
    std::vector<char> receive() const;
    bool hasMessageWaiting() const;

private:
    SocketImpl(SOCKET s, bool isConnectionListen, bool isClientListener);
    void init(const std::string& ipAddress, const std::string& portNumber);
    void startup() const;
    addrinfo* getAddressInfo(const std::string& ipAddress, const std::string& portNumber);
    SOCKET createSocket(addrinfo* addressInfo);
    void setSocketOptions(/*Future args here*/);
    void listen(const std::string& ipAddress, const std::string& portNumber);
    SOCKET accept() const;

private:
    SOCKET          socket_;
    addrinfo*       addressInfo_;
    bool            isConnectionListener_;
    bool            isClientListener_;
    bool            isInitialized_;
};


SocketImpl::SocketImpl()
    : socket_()
    , addressInfo_()
    , isConnectionListener_(false)
    , isClientListener_(false)
    , isInitialized_(false)
{
}

SocketImpl::SocketImpl(SocketImpl&& rhs) noexcept
    : socket_(rhs.socket_)
    , addressInfo_(rhs.addressInfo_)
    , isConnectionListener_(rhs.isConnectionListener_)
    , isClientListener_(rhs.isClientListener_)
    , isInitialized_(rhs.isInitialized_)
{
    rhs.addressInfo_ = nullptr;
    rhs.socket_ = 0;
}

SocketImpl::~SocketImpl()
{
    closesocket(socket_);

    if(addressInfo_)
    {
        freeaddrinfo(addressInfo_);
    }

    if(isInitialized_)
    {
        WSACleanup();
    }
}

SocketImpl& SocketImpl::operator=(SocketImpl&& rhs) noexcept 
{
    socket_ = rhs.socket_;
    addressInfo_ = rhs.addressInfo_;
    isConnectionListener_ = rhs.isConnectionListener_;
    isClientListener_ = rhs.isClientListener_;
    isInitialized_ = rhs.isInitialized_;

    rhs.addressInfo_ = nullptr;
    rhs.socket_ = 0;

    return *this;
}

SocketImpl::SocketImpl(SOCKET s, bool isConnectionListener, bool isClientListener)
    : socket_(s)
    , addressInfo_(nullptr)
    , isConnectionListener_(isConnectionListener)
    , isClientListener_(isClientListener)
    , isInitialized_(true)
{
    
}

SocketImpl::Ptr SocketImpl::waitForClient(const std::string& ipAddress, const std::string& portNumber)
{
    listen(ipAddress, portNumber);
    const auto rawSocket = accept();
    return std::unique_ptr<SocketImpl>(new SocketImpl(rawSocket, false, true)); //<raw new used to keep this constructor private
}

void SocketImpl::connectTo(const std::string& ipAddress, const std::string& portNumber)
{
    if(!isInitialized_)
    {
        init(ipAddress, portNumber);
    }

    const auto connectResult = ::connect(socket_, addressInfo_->ai_addr, static_cast<int>(addressInfo_->ai_addrlen));

    if(connectResult == SOCKET_ERROR)
    {
        throw CovidException("Unable to connect to socket");
    }

    if(socket_ == INVALID_SOCKET)
    {
        throw InvalidSocketException("Invalid socket");
    }
}

void SocketImpl::send(const std::vector<char>& message) const
{
    ::send(socket_, message.data(), static_cast<int>(message.size()), 0);
}

std::vector<char> SocketImpl::receive() const
{
    char buffer[256];

    const auto bytesReceived = recv(socket_, buffer, 256 * sizeof(char), 0);

    if(bytesReceived == - 1)
    {
        char errorBuffer[30];
        sprintf_s(errorBuffer, "%i", WSAGetLastError());
        throw CovidException("Unable to read from socket" + std::string(errorBuffer));
    }

    return std::vector<char>(std::begin(buffer), std::begin(buffer) + bytesReceived);
}

bool SocketImpl::hasMessageWaiting() const
{
    unsigned long bytesWaiting = 0;
    ioctlsocket(socket_, FIONREAD, &bytesWaiting);

    return bytesWaiting != 0;
}

void SocketImpl::init(const std::string& ipAddress, const std::string& portNumber)
{
    startup();
    addressInfo_ = getAddressInfo(ipAddress, portNumber);
    socket_ = createSocket(addressInfo_);
    setSocketOptions();
    isInitialized_ = true;
}

void SocketImpl::startup() const
{
    WSADATA wsaData;
    const auto startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if(startupResult != 0)
    {
        throw WsaFailedException("WsaStartup failed");
    }
}

addrinfo* SocketImpl::getAddressInfo(const std::string& ipAddress, const std::string& portNumber)
{
    addrinfo* info;
    addrinfo hints = {};

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    const auto addrInfoResult = getaddrinfo(ipAddress.c_str(), portNumber.c_str(), &hints, &info);

    if(addrInfoResult != 0)
    {
        throw CovidException("Unable to get ip Address info");
    }

    return info;
}

SOCKET SocketImpl::createSocket(addrinfo* addressInfo)
{
    const auto s = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);

    if(s == INVALID_SOCKET)
    {
        throw InvalidSocketException("Unable to create socket");
    }

    return s;
}

void SocketImpl::setSocketOptions(/*Future args here*/)
{
    int yes = 1; //<this is just a configuration. But, we need a pointer to it so it must be stored in a variable

    if(setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&yes), sizeof(int)) == -1)
    {
        throw CovidException("unable to set socket options");
    }
}

void SocketImpl::listen(const std::string& ipAddress, const std::string& portNumber)
{
    if(!isInitialized_)
    {
        init(ipAddress, portNumber);
    }

    const auto bindResult = bind( socket_, addressInfo_->ai_addr, static_cast<int>(addressInfo_->ai_addrlen));

    // Setup the TCP listening socket
    if (bindResult == SOCKET_ERROR) 
    {
        throw CovidException("Could not bind the socket");
    }

    const auto listenResult = ::listen(socket_, SOMAXCONN);

    if (listenResult == SOCKET_ERROR) 
    {
        throw CovidException("Listen failed");
    }
}

SOCKET SocketImpl::accept() const
{
    const auto clientSocket = ::accept(socket_, NULL, NULL);

    if (clientSocket == INVALID_SOCKET) 
    {
        throw CovidException("Accept failed");
    }

    return clientSocket;
}

Port::Port(const std::string& ipAddress, const std::string& portNumber)
    : ipAddress_(ipAddress)
    , portNumber_(portNumber)
    , portListener_(std::make_unique<SocketImpl>())
{
    
}

Port::Port(Port&& rhs) noexcept = default;
Port::~Port() = default;
Port& Port::operator=(Port&& rhs) noexcept = default;

std::string Port::ipAddress() const noexcept
{
    return ipAddress_;
}

std::string Port::portNumber() const noexcept
{
    return portNumber_;
}

SocketImpl::Ptr Port::listen() const
{
    return portListener_->waitForClient(ipAddress_, portNumber_);
}

Socket Socket::listenOn(const Port& port)
{
    return Socket(port.listen());
}

Socket Socket::connectTo(const Port& port)
{
    auto impl = std::make_unique<SocketImpl>();
    impl->connectTo(port.ipAddress(), port.portNumber());
    return Socket(std::move(impl));
}

Socket::Socket(Socket&& rhs)
{
    socket_ = std::move(rhs.socket_);
}

Socket::~Socket() = default;

Socket& Socket::operator=(Socket&& rhs)
{
    socket_ = std::move(rhs.socket_);
    return *this;
}

Socket::Socket(std::unique_ptr<SocketImpl>&& socket)
    : socket_(std::move(socket))
{
    
}

void Socket::send(const std::vector<char>& message) const
{
    socket_->send(message);
}

std::vector<char> Socket::receive() const
{
    return socket_->receive();
}

bool Socket::hasMessageWaiting() const
{
    return socket_->hasMessageWaiting();
}

uint16_t toNetworkByteOrder(uint16_t value)
{
    static_assert(sizeof(uint16_t) == sizeof(u_short));
    return htons(value);
}

uint32_t toNetworkByteOrder(uint32_t value)
{
    static_assert(sizeof(uint32_t) == sizeof(u_long));
    return htonl(value);
}

uint16_t fromNetworkByteOrder(uint16_t value)
{
    static_assert(sizeof(uint16_t) == sizeof(u_short));
    return ntohs(value);
}

uint32_t fromNetworkByteOrder(uint32_t value)
{
    static_assert(sizeof(uint32_t) == sizeof(u_long));
    return ntohl(value);
}
