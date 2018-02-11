//
// Created by richard on 06/02/18.
//

#ifndef EZNETWORK_SOCKET_H
#define EZNETWORK_SOCKET_H

#include <iostream>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <list>
#include <utility>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

namespace eznet
{
        /**
         * @brief Try to determine how many characters are availalbe in the input stream without blocking.
         * @return >0 the number of characters know to be available, 0 no information, -1 sequence unavailable
         */
        streamsize showmanyc() override {
            ssize_t n = ::recv(sockfd, ibuf + pushback_size, buffer_size - pushback_size, MSG_DONTWAIT);

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    n = 0;
                } else {
                    return -1;
                }
            }
            this->setg(ibuf, ibuf + pushback_size, ibuf + pushback_size + n);
            return n;
        }
    };


    /**
     * @brief How sockets should be selected. A bitwise OR mask may be used.
     */
    enum SelectClients
    {
        SC_None = 0,        ///< No connected sockets are slected
        SC_Read = 1,        ///< Select for read
        SC_Write = 2,       ///< Select for write
        SC_Except = 4,      ///< Select for exception
        SC_All = 7          ///< Select for all
    };


    /**
     * @brief The type of a socket
     */
    enum SocketType {
        SockUnknown,    ///< Socket type is not known.
        SockListen,     ///< Socket is a listening or server socket
        SockConnect,    ///< Socket is a connecting or client socket
        SockAccept,     ///< Socket is an accepted connection
    };


    /**
     * @brief How the socket should be shutdown
     */
    enum SocketHow {
        SHUT_RD = 0,    ///< Further reception disabled
        SHUT_WR = 1,    ///< Further transmission disabled
        SHUT_RDWR = 2,  ///< Further reception and transmission disabled
    };

    /**
       @mainpage

       # Socket #

       The Socket class provides for the creation and lifetime management of the three main types
       of stream sockets used by applications:
        - SockListen: Used by servers to accept connections from clients
        - SockConnect: Used by clients to connect to servers
        - SockAccept: The server side of a SockConnect connection

       ## SockAccept ##

       The creation of an accept socket is the simplest. When a call to ::accept(2) returns
       a valid file descriptor and peer address these values are passed to the constructor with
       the a prototype:

       `Socket(int fd, struct sockaddr *addr, socklen_t len)`

       This encapsulates the socket data in a Socket object.

       ## SockConnect ##

       The creation of a connection socket starts with the construction of a Socket object with
       the second constructor:

       `Socket(std::string host, std::string port)`

       _host_ provides the host name or address of the host the socket will connect to.

       _port_ provides the service name or port number the socket will connect to.

       The connection is established using the `connect(int ai_family_preference = AF_UNSPEC)` method.
       This converts the `host` and `port` parameters into a list of address specifications which may
       be any of the supported families available. The list is iterated until a connection in the
       family specified by `ai_family_preference` that accepts a connection is found. If `ai_family_preference`
       is `AF_UNSPEC` then all connections are tried. If one of the supported families is specified
       and none of the matching connections succeed all others are tried. If none of the potential
       connections succeed the method returns -1, if one succeeds the connection file descriptor
       is returned.

       ## SockListen ##

       The creation of a listener socket follows the the connection socket creation flow except for
       the following differences:
         - _host_ is resolved to map to an interface on the local system. If _host_ is an empty string
         it resolves to _any_ address.
         - The method `listen(int backlog, int ai_family_preference = AF_UNSPEC)` is called instead of `connect()`

       The IPV6 _any_ address is special in that it will accept connections using both IPV6 and IPV4
     */

    class Socket
    {
        friend class socket_streambuf;

    public:
        SelectClients selectClients;    ///< How this socket should be selected.

        Socket& operator = (const Socket &) = delete;
        Socket& operator = (Socket && other) noexcept {
            peer_host = other.peer_host;
            peer_port = other.peer_port;
            error_str = other.error_str;
            sock_fd = other.sock_fd;
            status = other.status;
            af_type = other.af_type;
            hints = std::move(other.hints);
            peer_info = other.peer_info;
            memcpy(&peer_addr, &other.peer_addr, other.peer_len);
            peer_len = other.peer_len;
            socket_type = other.socket_type;

            other.peer_host.clear();
            other.peer_port.clear();
            other.error_str.clear();
            other.peer_info = nullptr;
            other.sock_fd = -1;
            other.status = 0;
            other.af_type = AF_UNSPEC;
            other.peer_len = 0;
            other.socket_type = SockUnknown;
        }

    protected:
        string  peer_host,      ///< The user provided peer host name or address.
                peer_port,      ///< The user provided peer port or service name.
                error_str;      ///< The last error message collected.

        int     sock_fd,        ///< The socket file descriptor
                status,         ///< Status of some called messages
                af_type;        ///< The address family of the socket

        struct addrinfo *peer_info;

        unique_ptr<struct addrinfo> hints;  ///< Connection hints

        struct sockaddr_storage peer_addr;  ///< Storage of the peer address used to connect
        socklen_t peer_len;                 ///< The length of the peer address storage
        SocketType socket_type;             ///< The type of socket

        /**
         * @brief This method does the bulk of the work to complete realization of a socket.
         * @param bind_connect either ::bind() for a server or ::connect() for a client.
         * @param ai_family_preference the preferred address family AF_INET, AF_INET6 or AF_UNSPEC
         */
        void findPeerInfo(int (*bind_connect)(int, const struct sockaddr *, socklen_t),
                          list<int>& ai_family_preference) {

            // Loop over preferences
            for (auto pref: ai_family_preference) {
                // And each discovered connection possibility
                for (struct addrinfo *peer = peer_info; peer != nullptr; peer = peer->ai_next) {
                    // Apply preference
                    if (pref == AF_UNSPEC || pref == peer->ai_family) {

                        // Create a compatible socket
                        sock_fd = ::socket(peer->ai_family, peer->ai_socktype, peer->ai_protocol);

                        /**
                         * Either bind or connect the socket. On error collect the message,
                         * close the socket and set it to error condition. Try the next
                         * connection or return error.
                         */
                        if (bind_connect(sock_fd, peer->ai_addr, peer->ai_addrlen)) {
                            error_str = strerror(errno);
                            ::close(sock_fd);
                            sock_fd = -1;
                        } else {
                            /**
                             * Store the selected peer address
                             */
                            memcpy(&peer_addr, peer->ai_addr, peer->ai_addrlen);
                            peer_len = peer->ai_addrlen;
                            af_type = peer->ai_family;

                            /**
                             * Allow socket reuse.
                             */
                            int on{1};
                            status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on));

                            break;
                        }
                    }
                }

                if (sock_fd >= 0)
                    break;
            }
        }

    public:

        /**
         * @brief Create a socket object to hold an accepted connection.
         */
        explicit Socket(int fd,                     ///< The accepted connection file descriptor
                        struct sockaddr *addr,      ///< The peer address
                        socklen_t len               ///< The size of the peer address
        ) :
                selectClients{SelectClients::SC_Read},
                peer_host{},
                peer_port{},
                error_str{},
                sock_fd{fd},
                status{},
                hints{new struct addrinfo()},
                peer_info{nullptr},
                af_type{addr->sa_family},
                peer_addr{},
                socket_type{SockAccept},
                peer_len{}
        {
            memcpy(&peer_addr, addr, len);
            peer_len = len;
        }

        /**
         * @brief Create a socket object for a new socket connection.
         */
        explicit Socket(string host,                ///< The hostname or address to connect or bind to
                        string port                 ///< The port number to connect or bind to
        ) :
                selectClients{SelectClients::SC_Read},
                peer_host{std::move(host)},
                peer_port{std::move(port)},
                error_str{},
                sock_fd{-1},
                status{},
                hints{new struct addrinfo()},
                peer_info{nullptr},
                af_type{AF_UNSPEC},
                peer_addr{},
                socket_type{SockUnknown},
                peer_len{}
        {
            init();
        }

        /**
         * @brief Destroy a socket, closing the file descriptor if it is open.
         */
        ~Socket() {
            if (sock_fd >= 0)
                close();
            if (peer_info)
                freeaddrinfo(peer_info);
        }

        /**
         * @brief Set or change the host specification post creation.
         */
        void setHost(const string &host /**< The hostname or address to connect or bind to */) { peer_host = host; }

        /**
         * @brief Set or change the port specification post creation.
         */
        void setPort(const string &port /**< The port number to connect or bind to */) { peer_port = port; }

        /**
         * @brief Initialize the object after any of the preceeding setters has been called.
         * Strong exception safety.
         */
        void init() {

            memset(hints.get(), 0, sizeof(hints));

            hints->ai_flags = AF_UNSPEC;
            hints->ai_socktype = SOCK_STREAM;
            hints->ai_flags = AI_PASSIVE;

            if ((status = getaddrinfo( (peer_host.length() ? peer_host.c_str() : nullptr),
                                       peer_port.c_str(), hints.get(), &peer_info))) {
                freeaddrinfo(peer_info);
                peer_info = nullptr;
                memset(&hints, 0, sizeof(hints));
                throw logic_error(string{"getaddrinfo error: "} + gai_strerror(status));
            }

            sock_fd = -1;
            socket_type = SockUnknown;
        }


        /**
         * @brief Get the socket file descriptor
         * @return the file descriptor
         */
        int fd() const { return sock_fd; }


        /**
         * @brief Get the type of the socket
         * @return A SocketType value
         */
        SocketType socketType() const { return socket_type; }


        /**
         * @brief Determine if the socket is open
         * @return true if open
         */
        explicit operator bool () const { return sock_fd >= 0; }


        /**
         * @brief Get the last set status return value for the socket
         * @return an integer status value
         */
        int getStatus() const { return status; }


        /**
         * @brief Set a status value returned by a function called on the socket
         * @param s an integer status value
         */
        void setStatus(int s) { status = s; }


        /**
         * @brief Complete a socket as a connection or client socket
         * @tparam AiFamilyPrefs A template parameter pack for a list of AF families
         * @param familyPrefs A list of AF family values AF_INET6, AF_INET, AF_UNSPEC
         * @return the socket fd or -1 on error
         */
        template <typename... AiFamilyPrefs>
        int connect(AiFamilyPrefs... familyPrefs) {
            list<int> prefsList{};
            (prefsList.push_back(familyPrefs), ...);

            findPeerInfo( ::connect, prefsList );

            freeaddrinfo(peer_info);
            peer_info = nullptr;

            socket_type = SockConnect;
            return sock_fd;
        }


        /**
         * @brief Complet a socket as a listen or server socket
         * @tparam AiFamilyPrefs A template parameter pack for a list of AF families
         * @param backlog the parameter passed to listen(2) as backlog
         * @param familyPrefs A list of AF family values AF_INET6, AF_INET, AF_UNSPEC
         * @return the socket fd or -1 on error
         * @details Finds a connection specification that allows a socket to be created
         * and bound preferring the provided address family preference, if any.
         */
        template <typename... AiFamilyPrefs>
        int listen(int backlog, AiFamilyPrefs... familyPrefs) {
            list<int> prefsList{};
            (prefsList.push_back(familyPrefs), ...);

            findPeerInfo( ::bind, prefsList );

            freeaddrinfo(peer_info);
            peer_info = nullptr;

            if (sock_fd >= 0)
                ::listen(sock_fd, backlog);

            socket_type = SockListen;
            return sock_fd;
        }


        /**
         * @brief Generate a peer address string for the socket.
         * @param flags Flags that will be passed to getnameinfo()
         * @return a string with the form <host>:<service>
         * @details For Sockets of type SockListen the 'peer' is the hostname of the interface the
         * Socket is listening to. For other types it is the hostname of the remote machine.
         */
        string getPeerName(int flags = NI_NOFQDN | NI_NUMERICSERV) {
            string result;
            char hbuf[NI_MAXHOST];
            char sbuf[NI_MAXSERV];
            if (getnameinfo((const sockaddr*)&peer_addr, peer_len,
                            hbuf, sizeof(hbuf),
                            sbuf, sizeof(sbuf),
                            flags) == 0) {
                result = string{hbuf} + ':' + sbuf;
            }

            return result;
        }


        /**
         * @brief Close a socket and set the internal file descriptor to -1;
         * @return the return value from ::close(2)
         */
        int close() {
            int r = ::close(sock_fd);
            sock_fd = -1;
            return r;
        }


        /**
         * @brief Shutdown a socket
         * @param how One of SHUT_RD, SHUT_WR or SHUT_RDWR
         * @return the return value from ::shutdown(2)
         */
        int shutdown(SocketHow how) {
            return ::shutdown(sock_fd, how);
        }


        /**
         * @brief Returns the last error message collected, if any.
         * @return a string
         */
        const string &errorString() const {
            return error_str;
        }
    };

    /**
     * @brief A streambuf which abstracts the socket file descriptor allowing the use of
     * standard iostreams.
     */
    class socket_streambuf : public std::streambuf
    {
    public:

        typedef char char_type;                             ///< The character type supported
        typedef std::char_traits<char_type> traits_type;    ///< Character traits for char_type
        typedef typename traits_type::int_type int_type;    ///< Integer type

        constexpr static size_t buffer_size = BUFSIZ;       ///< System specified size of buffers
        constexpr static size_t pushback_size = 8;          ///< The minimum number of characters that may be pushed back

        socket_streambuf() = delete;

        /**
         * @brief Create a socket stream buffer interfaced to a Socket object file descriptor.
         * @param sock
         */
        explicit socket_streambuf(Socket &sock) : socket(sock), obuf{}, ibuf{} {
            this->setp(obuf, obuf+buffer_size);
            this->setg(ibuf, ibuf+pushback_size, ibuf+pushback_size);
        }

    protected:
        Socket &socket;                                 ///< The Socket object this buffer interfaces with
        char_type obuf[buffer_size];                    ///< The output stream buffer
        char_type ibuf[buffer_size+pushback_size];      ///< The input stream buffer and pushback space

        /**
         * @brief Flush the contents of the output buffer to the Socket
         * @return 0 on success, -1 on failure.
         */
        int sync() override {
            if (socket.sock_fd >= 0) {
                ssize_t n = ::send(socket.sock_fd, obuf, pptr() - obuf, 0 );

                if (n < 0) {
                    return -1;
                } else if (n == (pptr() - obuf)) {
                    setp(obuf, obuf + buffer_size);
                } else {
                    for (ssize_t cp = n; cp < (buffer_size - n); ++cp) {
                        obuf[cp - n] = obuf[cp];
                    }
                    setp(obuf, obuf + buffer_size);
                    pbump(static_cast<int>(buffer_size-n));
                }
                return 0;
            }
            return -1;
        }

        /**
         * @brief Called when output data won't fit in the output buffer
         * @details The output buffer is flushed to the Socket. The overflow character is placed in the clean
         * buffer.
         * @param c The character that caused the overflow
         * @return the input character
         */
        int_type overflow( int_type c ) override {
            if (sync() < 0)
                return traits_type::eof();

            if (traits_type::not_eof(c)) {
                char_type cc = traits_type::to_char_type(c);
                this->xsputn(&cc, 1);
            }

            return c;
        }

        /**
         * @brief Called when there is not enough data in the input buffer to satisfy an request
         * @details More data is read from the underlying Socket. If none is available return EOF,
         * otherwise return the first new character.
         * @return The next available character or EOF
         */
        int_type underflow( ) override {
            if (socket.sock_fd >= 0) {
                ssize_t n = ::recv(socket.sock_fd, ibuf + pushback_size, buffer_size - pushback_size, 0);

                if (n < 0) {
                    return traits_type::eof();
                }
                this->setg(ibuf, ibuf + pushback_size, ibuf + pushback_size + n);
                if (n) {
                    return traits_type::to_int_type(*(ibuf + pushback_size));
                }
            }
            return traits_type::eof();
        }
    };
}

#endif //EZNETWORK_SOCKET_H
