#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdexcept>

class CovidException : public std::runtime_error
{
public:
    CovidException(const std::string& reason)
        : std::runtime_error(reason)
    {

    }
};

class WsaFailedException : public CovidException
{
public:
    WsaFailedException(const std::string& reason)
        : CovidException(reason)
    {

    }
};

class InvalidSocketException : public CovidException
{
public:
    InvalidSocketException(const std::string& reason)
        : CovidException(reason)
    {

    }
};

#endif //EXCEPTION_H
