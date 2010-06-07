#ifndef __MORDOR_SOCKET_H__
#define __MORDOR_SOCKET_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "exception.h"
#include "iomanager.h"
#include "version.h"

#ifdef WINDOWS
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef OSX
# include <netinet/in_systm.h>
# include <netinet/ip.h>
#endif
#include <sys/un.h>
#endif

namespace Mordor {

#ifdef WINDOWS
struct iovec
{
    union
    {
        WSABUF wsabuf;
        struct {
            u_long iov_len;
            void *iov_base;
        };
    };
};
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
typedef u_long iov_len_t;
typedef SOCKET socket_t;

class EventLoop;
#else
typedef size_t iov_len_t;
typedef int socket_t;
#endif

struct SocketException : virtual NativeException {};

struct AddressInUseException : virtual SocketException {};
struct ConnectionAbortedException : virtual SocketException {};
struct ConnectionResetException : virtual SocketException {};
struct ConnectionRefusedException : virtual SocketException {};
struct HostDownException : virtual SocketException {};
struct HostUnreachableException : virtual SocketException {};
struct NetworkDownException : virtual SocketException {};
struct NetworkResetException : virtual SocketException {};
struct NetworkUnreachableException : virtual SocketException {};
struct TimedOutException : virtual SocketException {};

struct Address;

class Socket : public boost::enable_shared_from_this<Socket>, boost::noncopyable
{
public:
    typedef boost::shared_ptr<Socket> ptr;
private:
    Socket(IOManager *ioManager, int family, int type, int protocol, int initialize);
#ifdef WINDOWS
    Socket(EventLoop *eventLoop, int family, int type, int protocol, int initialize);
#endif
public:
    Socket(int family, int type, int protocol = 0);
    Socket(IOManager &ioManager, int family, int type, int protocol = 0);
#ifdef WINDOWS
    Socket(EventLoop &eventLoop, int family, int type, int protocol = 0);
#endif
    ~Socket();

    unsigned long long receiveTimeout() { return m_receiveTimeout; }
    void receiveTimeout(unsigned long long us) { m_receiveTimeout = us; }
    unsigned long long sendTimeout() { return m_sendTimeout; }
    void sendTimeout(unsigned long long us) { m_sendTimeout = us; }

    void bind(const Address &addr);
    void bind(const boost::shared_ptr<Address> addr);
    void connect(const Address &to);
    void connect(const boost::shared_ptr<Address> addr)
    { connect(*addr.get()); }
    void listen(int backlog = SOMAXCONN);

    Socket::ptr accept();
    void accept(Socket &target);
    void shutdown(int how = SHUT_RDWR);

    void getOption(int level, int option, void *result, size_t *len);
    void setOption(int level, int option, const void *value, size_t len);

    void cancelAccept();
    void cancelConnect();
    void cancelSend();
    void cancelReceive();

    size_t send(const void *buffer, size_t length, int flags = 0);
    size_t send(const iovec *buffers, size_t length, int flags = 0);
    size_t sendTo(const void *buffer, size_t length, int flags, const Address &to);
    size_t sendTo(const void *buffer, size_t length, int flags, const boost::shared_ptr<Address> to)
    { return sendTo(buffer, length, flags, *to.get()); }
    size_t sendTo(const iovec *buffers, size_t length, int flags, const Address &to);
    size_t sendTo(const iovec *buffers, size_t length, int flags, const boost::shared_ptr<Address> to)
    { return sendTo(buffers, length, flags, *to.get()); }

    size_t receive(void *buffer, size_t length, int *flags = NULL);
    size_t receive(iovec *buffers, size_t length, int *flags = NULL);
    size_t receiveFrom(void *buffer, size_t length, Address &from, int *flags = NULL);
    size_t receiveFrom(iovec *buffers, size_t length, Address &from, int *flags = NULL);

    boost::shared_ptr<Address> emptyAddress();
    boost::shared_ptr<Address> remoteAddress();
    boost::shared_ptr<Address> localAddress();

    int family() { return m_family; }
    int type();
    int protocol() { return m_protocol; }

private:
    template <bool isSend>
    size_t doIO(iovec *buffers, size_t length, int &flags, Address *address = NULL);

#ifdef WINDOWS
    void cancelIo(int event, error_t &cancelled, error_t error);
#else
    void cancelIo(IOManager::Event event, error_t &cancelled, error_t error);
#endif

private:
    socket_t m_sock;
    int m_family, m_protocol;
    IOManager *m_ioManager;
    unsigned long long m_receiveTimeout, m_sendTimeout;
    error_t m_cancelledSend, m_cancelledReceive;
    boost::shared_ptr<Address> m_localAddress, m_remoteAddress;
#ifdef WINDOWS
    EventLoop *m_eventLoop;
    bool m_skipCompletionPortOnSuccess;
    // All this, just so a connect/accept can be cancelled on win2k
    bool m_unregistered;
    HANDLE m_hEvent;
    Fiber::ptr m_fiber;
    Scheduler *m_scheduler;

    AsyncEvent m_sendEvent, m_receiveEvent;
#endif
};

#ifdef WINDOWS
typedef errinfo_lasterror errinfo_gaierror;
#else
typedef boost::error_info<struct tag_gaierror, int> errinfo_gaierror;
std::string to_string( errinfo_gaierror const & e );
#endif

struct NameLookupException : virtual SocketException {};
struct TemporaryNameServerFailureException : virtual NameLookupException {};
struct PermanentNameServerFailureException : virtual NameLookupException {};
struct NoNameServerDataException : virtual NameLookupException {};
struct HostNotFoundException : virtual NameLookupException {};

struct Address
{
public:
    typedef boost::shared_ptr<Address> ptr;
protected:
    Address(int type, int protocol = 0);
public:
    virtual ~Address() {}

    static std::vector<ptr>
        lookup(const std::string& host, int family = AF_UNSPEC,
            int type = 0, int protocol = 0);
    static ptr create(const sockaddr *name, socklen_t nameLen,
        int type = 0, int protocol = 0);

    Socket::ptr createSocket();
    Socket::ptr createSocket(IOManager &ioManager);
#ifdef WINDOWS
    Socket::ptr createSocket(EventLoop &eventLoop);
#endif

    int family() const { return name()->sa_family; }
    int type() const { return m_type; }
    int protocol() const { return m_protocol; }
    virtual const sockaddr *name() const = 0;
    virtual sockaddr *name() = 0;
    virtual socklen_t nameLen() const = 0;
    virtual std::ostream & insert(std::ostream &os) const;

    bool operator<(const Address &rhs) const;
    bool operator==(const Address &rhs) const;
    bool operator!=(const Address &rhs) const;

private:
    int m_type, m_protocol;
};

struct IPAddress : public Address
{
public:
    typedef boost::shared_ptr<IPAddress> ptr;

protected:
    IPAddress(int type = 0, int protocol = 0);
public:
    virtual unsigned short port() const = 0;
    virtual void port(unsigned short p) = 0;
};

struct IPv4Address : public IPAddress
{
public:
    IPv4Address(int type = 0, int protocol = 0);
    //IPv4Address(const std::string& addr, int type = 0, int protocol = 0);
    //IPv4Address(const std::string& addr, unsigned short port, int type = 0, int protocol = 0);

    unsigned short port() const { return ntohs(sin.sin_port); }
    void port(unsigned short p) { sin.sin_port = htons(p); }

    const sockaddr *name() const { return (sockaddr*)&sin; }
    sockaddr *name() { return (sockaddr*)&sin; }
    socklen_t nameLen() const { return sizeof(sockaddr_in); }

    std::ostream & insert(std::ostream &os) const;
private:
    sockaddr_in sin;
};

struct IPv6Address : public IPAddress
{
public:
    IPv6Address(int type = 0, int protocol = 0);
    //IPv6Address(const std::string& addr, int type = 0, int protocol = 0);
    //IPv6Address(const std::string& addr, unsigned short port, int type = 0, int protocol = 0);

    unsigned short port() const { return ntohs(sin.sin6_port); }
    void port(unsigned short p) { sin.sin6_port = htons(p); }

    const sockaddr *name() const { return (sockaddr*)&sin; }
    sockaddr *name() { return (sockaddr*)&sin; }
    socklen_t nameLen() const { return sizeof(sockaddr_in6); }

    std::ostream & insert(std::ostream &os) const;
private:
    sockaddr_in6 sin;
};

#ifndef WINDOWS
struct UnixAddress : public Address
{
public:
    UnixAddress(const std::string &path, int type = 0, int protocol = 0);

    const sockaddr *name() const { return (sockaddr*)&sun; }
    sockaddr *name() { return (sockaddr*)&sun; }
    socklen_t nameLen() const { return length; }

    std::ostream & insert(std::ostream &os) const;

private:
    size_t length;
    struct sockaddr_un sun;
};
#endif

struct UnknownAddress : public Address
{
public:
    UnknownAddress(int family, int type = 0, int protocol = 0);

    const sockaddr *name() const { return &sa; }
    sockaddr *name() { return &sa; }
    socklen_t nameLen() const { return sizeof(sockaddr); }
private:
    sockaddr sa;
};

std::ostream &operator <<(std::ostream &os, const Address &addr);

bool operator<(const Address::ptr &lhs, const Address::ptr &rhs);

}

#endif
