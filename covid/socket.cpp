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
    SocketImpl(const SocketImpl& rhs);
    SocketImpl(SocketImpl&& rhs) noexcept;
    ~SocketImpl();

    SocketImpl& operator=(const SocketImpl& rhs) noexcept;
    SocketImpl& operator=(SocketImpl&& rhs) noexcept;

    Ptr waitForClient(const std::string& ipAddress, const std::string& portNumber);
    std::vector<char> receive() const;

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

SocketImpl::SocketImpl(const SocketImpl& rhs)
    : socket_(rhs.socket_)
    , addressInfo_(nullptr)
    , isConnectionListener_(rhs.isConnectionListener_)
    , isClientListener_(rhs.isClientListener_)
    , isInitialized_(rhs.isInitialized_)
{
}

SocketImpl::SocketImpl(SocketImpl&& rhs) noexcept
    : socket_(rhs.socket_)
    , addressInfo_(rhs.addressInfo_)
    , isConnectionListener_(rhs.isConnectionListener_)
    , isClientListener_(rhs.isClientListener_)
    , isInitialized_(rhs.isInitialized_)
{
    
}

SocketImpl::~SocketImpl()
{
    closesocket(socket_);

    if(addressInfo_)
    {
        freeaddrinfo(addressInfo_);
    }

    WSACleanup();
}

SocketImpl& SocketImpl::operator=(const SocketImpl& rhs) noexcept
{
    socket_ = rhs.socket_;
    addressInfo_ = nullptr;
    isConnectionListener_ = rhs.isConnectionListener_;
    isClientListener_ = rhs.isClientListener_;
    isInitialized_ = rhs.isInitialized_;
    return *this;
}

SocketImpl& SocketImpl::operator=(SocketImpl&& rhs) noexcept 
{
    socket_ = rhs.socket_;
    addressInfo_ = rhs.addressInfo_;
    isConnectionListener_ = rhs.isConnectionListener_;
    isClientListener_ = rhs.isClientListener_;
    isInitialized_ = rhs.isInitialized_;
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

std::vector<char> SocketImpl::receive() const
{
    char buffer[5];

    const auto bytesReceived = recv(socket_, buffer, 5 * sizeof(char), 0);

    if(bytesReceived == - 1)
    {
        throw CovidException("Unable to read from socket");
    }

    return std::vector<char>(std::begin(buffer), std::end(buffer));
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

ServerSocket::ServerSocket()
    : impl_(std::make_unique<SocketImpl>())
{
    
}

ServerSocket::ServerSocket(const ServerSocket& rhs)
{
   if(!rhs.impl_)
   {
       throw CovidException("rhs has no impl");
   }

   if(!impl_)
   {
       impl_ =std::make_unique<SocketImpl>();
   }

   *impl_ = *rhs.impl_;
}

ServerSocket& ServerSocket::operator=(const ServerSocket& rhs)
{
   if(!rhs.impl_)
   {
       throw CovidException("rhs has no impl");
   }

   if(!impl_)
   {
       impl_ =std::make_unique<SocketImpl>();
   }

   *impl_ = *rhs.impl_;

   return *this;
}

ServerSocket::~ServerSocket() = default;

void ServerSocket::listen(const std::string& ipAddress, const std::string& portNumber)
{
    if(!impl_)
    {
        throw InvalidSocketException("listen called on invalid socket");
    }

    client_ = impl_->waitForClient(ipAddress, portNumber);
}

std::vector<char> ServerSocket::receive()
{
    return client_->receive();
}

ClientSocket::~ClientSocket() = default;
