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
#include <netdb.h>

using namespace std;

namespace eznet
{
    /**
     * @brief The type of a socket
     */
    enum SocketType {
        SockUnknown,    ///< Socket type is not known.
        SockListen,     ///< Socket is a listening or server socket
        SockConnect,    ///< Socket is a connecting or client socket
        SockAccept,     ///< Socket is an accepted connection
    };


    enum SocketHow {
        SHUT_RD = 0,
        SHUT_WR = 1,
        SHUT_RDWR = 2,
    };

    /**
     * @brief A class to abstract a stream socket connection.
     */
    class Socket
    {
        friend class socket_streambuf;

    protected:
        string  peer_host,      ///< The user provided peer host name or address.
                peer_port,      ///< The user provided peer port or service name.
                error_str;      ///< The last error message collected.

        int     sockfd,         ///< The socket file descriptor
                status,         ///< Status of some called messages
                af_type;        ///< The address family of the socket

        struct addrinfo hints,  ///< Connection hints
                *peerinfo;      ///< Discovered peerinfo, freed after connection

        struct sockaddr_storage peer_addr;  ///< Storage of the peer address used to connect
        socklen_t peer_len;                 ///< The length of the peer address storage
        SocketType socketType;  ///< The type of socket

        /**
         * @brief This method does the bulk of the work to complete realization of a socket.
         * @param bind_connect either ::bind() for a server or ::connect() for a client.
         * @param ai_family_preference the preferred address family AF_INET, AF_INET6 or AF_UNSPEC
         */
        void findPeerInfo(int (*bind_connect)(int, const struct sockaddr *, socklen_t),
                          int ai_family_preference) {

            /**
             * Create a list of address families to iterate over to find the preferred family.
             * AF_UNSPEC is always at the end.
             */
            list<int> pref_list{ AF_UNSPEC };

            // Prepend the user perference if it is not AF_UNSPC, so we don't iterate unnecessarily
            if (ai_family_preference != AF_UNSPEC)
                pref_list.push_front( ai_family_preference );

            // Loop over preferences
            for (auto pref: pref_list) {
                // And each discovered connection possibility
                for (struct addrinfo *peer = peerinfo; peer != nullptr; peer = peer->ai_next) {
                    // Apply preference
                    if (pref == AF_UNSPEC || pref == peer->ai_family) {

                        // Create a compatible socket
                        sockfd = ::socket(peer->ai_family, peer->ai_socktype, peer->ai_protocol);

                        /**
                         * Either bind or connect the socket. On error collect the message,
                         * close the socket and set it to error condition. Try the next
                         * connection or return error.
                         */
                        if (bind_connect(sockfd, peer->ai_addr, peer->ai_addrlen)) {
                            error_str = strerror(errno);
                            ::close(sockfd);
                            sockfd = -1;
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
                            status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on));

                            break;
                        }
                    }
                }

                if (sockfd >= 0)
                    break;
            }
        }

    public:

        /**
         * @brief Create a socket.
         * @param host
         * @param port
         */
        explicit Socket(int fd, struct sockaddr *addr, socklen_t len) :
                peer_host{},
                peer_port{},
                error_str{},
                sockfd{fd},
                status{},
                hints{},
                peerinfo{},
                af_type{addr->sa_family},
                peer_addr{},
                socketType{SockAccept},
                peer_len{}
        {
            memcpy(&peer_addr, addr, len);
            peer_len = len;
        }

        /**
         * @brief Create a socket.
         * @param host
         * @param port
         */
        explicit Socket(string host, string port) :
                peer_host{std::move(host)},
                peer_port{std::move(port)},
                error_str{},
                sockfd{-1},
                status{},
                hints{},
                peerinfo{},
                af_type{AF_UNSPEC},
                peer_addr{},
                socketType{SockUnknown},
                peer_len{}
        {
            init();
        }

        ~Socket() {
            if (sockfd >= 0)
                close();
        }

        /**
         * @brief Set the host specification post creation.
         * @param host
         */
        void setHost(const string &host) { peer_host = host; }

        /**
         * @brief Set the port specification post creation.
         * @param port
         */
        void setPort(const string &port) { peer_port = port; }

        /**
         * @brief Initialize the object after any of the preceeding setters has been called.
         * Strong exception safety.
         */
        void init() {
            memset(&hints, 0, sizeof(hints));

            hints.ai_flags = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE;

            if ((status = getaddrinfo( (peer_host.length() ? peer_host.c_str() : nullptr),
                                       peer_port.c_str(), &hints, &peerinfo))) {
                freeaddrinfo(peerinfo);
                memset(&hints, 0, sizeof(hints));
                throw logic_error(string{"getaddrinfo error: "} + gai_strerror(status));
            }

            sockfd = -1;
            socketType = SockUnknown;
        }


        /**
         * @brief Get the socket file descriptor
         * @return the file descriptor
         */
        int fd() const { return sockfd; }


        /**
         * @brief Determine if the socket is open
         * @return true if open
         */
        explicit operator bool () const { return sockfd >= 0; }


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
         * @param ai_family_preference the address family preference
         * @return the socket fd or -1 on error
         * @details Finds a connection specification that allows a socket to be created
         * and connected preferring the provided address family preference, if any.
         */
        int connect(int ai_family_preference = AF_UNSPEC) {
            findPeerInfo( ::connect, ai_family_preference );

            freeaddrinfo(peerinfo);

            socketType = SockConnect;
            return sockfd;
        }


        /**
         * @brief Complet a socket as a listen or server socket
         * @param backlog the parameter passed to listen(2) as backlog
         * @param ai_family_preference the address family preference
         * @return the socket fd or -1 on error
         * @details Finds a connection specification that allows a socket to be created
         * and bound preferring the provided address family preference, if any.
         */
        int listen(int backlog, int ai_family_preference = AF_UNSPEC) {
            findPeerInfo( ::bind, ai_family_preference );

            freeaddrinfo(peerinfo);

            if (sockfd >= 0)
                ::listen(sockfd, backlog);

            socketType = SockListen;
            return sockfd;
        }


        /**
         * @brief Close a socket and set the internal file descriptor to -1;
         * @return the return value from ::close(2)
         */
        int close() {
            int r = ::close(sockfd);
            sockfd = -1;
            return r;
        }


        /**
         * @brief Shutdown a socket
         * @param how One of SHUT_RD, SHUT_WR or SHUT_RDWR
         * @return the return value from ::shutdown(2)
         */
        int shutdown(SocketHow how) {
            return ::shutdown(sockfd, how);
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

        typedef char char_type;
        typedef std::char_traits<char_type> traits_type;
        typedef typename traits_type::int_type int_type;

        constexpr static size_t buffer_size = BUFSIZ;
        constexpr static size_t pushback_size = 8;

        socket_streambuf() = delete;

        explicit socket_streambuf(Socket &sock) : socket(sock), obuf{}, ibuf{} {
            this->setp(obuf, obuf+buffer_size);
            this->setg(ibuf, ibuf+pushback_size, ibuf+pushback_size);
        }

    protected:
        Socket &socket;
        char_type obuf[buffer_size];
        char_type ibuf[buffer_size+pushback_size];

        int sync() override {
            if (socket.sockfd >= 0) {
                ssize_t n = ::send(socket.sockfd, obuf, pptr() - obuf, 0 );

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

        int_type overflow( int_type c ) override {
            if (sync() < 0)
                return traits_type::eof();

            if (traits_type::not_eof(c)) {
                char_type cc = traits_type::to_char_type(c);
                this->xsputn(&cc, 1);
            }

            return traits_type::to_int_type(c);
        }

        int_type underflow( ) override {
            if (socket.sockfd >= 0) {
                ssize_t n = ::recv(socket.sockfd, ibuf + pushback_size, buffer_size - pushback_size, 0);

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
