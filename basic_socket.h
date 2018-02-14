//
// Created by hrbuckl on 2/14/18.
//

#ifndef EZNETWORK_BASIC_SOCKET_H
#define EZNETWORK_BASIC_SOCKET_H

#include <iostream>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <future>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <utility>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

namespace async_net {
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


    class basic_socket {
    protected:
        string peer_host,       ///< The user provided peer host name or address.
                peer_port;      ///< The user provided peer port or service name.

        int sock_fd,            ///< The socket file descriptor
                status,         ///< Status of some called messages
                af_type;        ///< The address family of the socket

        SocketType socket_type;     ///< The type of socket

        struct sockaddr_storage peer_addr;  ///< Storage of the peer address used to connect
        socklen_t peer_len;                 ///< The length of the peer address storage

        basic_socket(string host, string port) :
                peer_host{std::move(host)},
                peer_port{std::move(port)},
                sock_fd{-1},
                socket_type{SockUnknown},
                status{0},
                af_type{AF_UNSPEC},
                peer_addr{},
                peer_len{sizeof(peer_addr)} {}

    public:

        /**
         * @brief Create a socket object to hold an accepted connection.
         */
        explicit basic_socket(int fd,                     ///< The accepted connection file descriptor
                              struct sockaddr *addr,      ///< The peer address
                              socklen_t len               ///< The size of the peer address
        ) :
                peer_host{},
                peer_port{},
                sock_fd{fd},
                socket_type{SockAccept},
                status{},
                af_type{addr->sa_family},
                peer_addr{},
                peer_len{} {
            memcpy(&peer_addr, addr, len);
            peer_len = len;
        }

        basic_socket &operator=(const basic_socket &) = delete;

        basic_socket &operator=(basic_socket &&other) noexcept {
            peer_host = std::move(other.peer_host);
            peer_port = std::move(other.peer_port);
            sock_fd = other.sock_fd;
            other.sock_fd = -1;
            af_type = other.af_type;
            other.af_type = AF_UNSPEC;
            peer_len = other.peer_len;
            memcpy(&peer_addr, &other.peer_addr, peer_len);
        }

    };

    struct local_socket : public basic_socket {
    public:
        local_socket(const string &host, const string &port) :
                basic_socket{host, port} {
        }


        /**
         * @brief Complete a socket as a connection or client socket
         * @tparam AiFamilyPrefs A template parameter pack for a list of AF families
         * @param familyPrefs A list of AF family values AF_INET6, AF_INET, AF_UNSPEC
         * @return the socket fd or -1 on error
         */
        template<typename... AiFamilyPrefs>
        int connect(AiFamilyPrefs... familyPrefs) {
            list<int> prefsList{};
            (prefsList.push_back(familyPrefs), ...);

            findPeerInfo(::connect, prefsList);

            socket_type = SockConnect;
            return sock_fd;
        }


        /**
         * @brief Complet a socket as a listen or server socket
         * @tparam AiFamilyPrefs A template parameter pack for a list of AF families
         * @param backlog the parameter passed to listen(2) as backlog
         * @param familyPrefs A list of AF family values AF_INET6, AF_INET, AF_UNSPEC
         * @return -1 on error, 0 on success
         * @details Finds a connection specification that allows a socket to be created
         * and bound preferring the provided address family preference, if any. If the
         * socket fd is successfully created this method also calls:
         *  - socketFlags(true, O_NONBLOCK)
         *  - closeOnExec(true)
         */
        template<typename... AiFamilyPrefs>
        int listen(int backlog, AiFamilyPrefs... familyPrefs) {
            int socketFlagSet = O_NONBLOCK;
            bool closeExec = true;

            list<int> prefsList{};
            (prefsList.push_back(familyPrefs), ...);

            findPeerInfo(::bind, prefsList);

            if (sock_fd >= 0) {
                ::listen(sock_fd, backlog);

                socket_type = SockListen;

                /**
                 * Allow socket reuse.
                 */
                int on{1};
                status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof(on));

                return std::min(socketFlags(true, socketFlagSet), closeOnExec(closeExec));
            }

            return -1;
        }


        /**
         * @brief Generate a peer address string for the socket.
         * @param flags Flags that will be passed to getnameinfo()
         * @return a string with the form <host>:<service>
         * @details For Sockets of type SockListen the 'peer' is the hostname of the interface the
         * Socket is listening to. For other types it is the hostname of the remote machine.
         */
        string getPeerName(unsigned int flags = NI_NOFQDN | NI_NUMERICSERV) {
            string result;
            char hbuf[NI_MAXHOST];
            char sbuf[NI_MAXSERV];
            if (getnameinfo((const sockaddr *) &peer_addr, peer_len,
                            hbuf, sizeof(hbuf),
                            sbuf, sizeof(sbuf),
                            flags) == 0) {
                result = string{hbuf} + ':' + sbuf;
            }

            return result;
        }


        /**
         * @brief Uses fcntl(2) to set or clearflags on the socket file descriptor.
         * @param set When true set the specified flags, otherwise clear.
         * @param flags An or mask of flags to set or clear
         * @return -1 on error, 0 on success, errno is set to indicate the error encountered.
         */
        int socketFlags(bool set, int flags) {
            if (sock_fd < 0) {
                errno = EBADF;
                return -1;
            }

            int oflags = fcntl(sock_fd, F_GETFL);
            if (oflags < 0)
                return -1;

            if (fcntl(sock_fd, F_SETFL, (set ? oflags | flags : oflags & (~flags))))
                return -1;

            return 0;
        }


        /**
         * @brief Uses fcntl(2) to set or clear FD_CLOEXEC flag
         * @param close When true set the flag, otherwise clear
         * @return -1 on error, 0 on success, errno is set to indicate the error encountered.
         */
        int closeOnExec(bool close) {
            if (sock_fd < 0) {
                errno = EBADF;
                return -1;
            }

            int oflags = fcntl(sock_fd, F_GETFD);
            if (oflags < 0)
                return -1;

            if (fcntl(sock_fd, F_SETFD, (close ? oflags | FD_CLOEXEC : oflags & (~FD_CLOEXEC))))
                return -1;

            return 0;
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


    protected:
        /**
         * @brief This method does the bulk of the work to complete realization of a socket.
         * @param bind_connect either ::bind() for a server or ::connect() for a client.
         * @param ai_family_preference the preferred address family AF_INET, AF_INET6 or AF_UNSPEC
         */
        void findPeerInfo(int (*bind_connect)(int, const struct sockaddr *, socklen_t),
                          list<int> &ai_family_preference) {

            struct addrinfo hints{};
            struct addrinfo *peer_info{nullptr};
            memset(&hints, 0, sizeof(hints));

            hints.ai_flags = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE;

            if ((status = getaddrinfo((peer_host.length() ? peer_host.c_str() : nullptr),
                                      peer_port.c_str(), &hints, &peer_info))) {
                freeaddrinfo(peer_info);
                peer_info = nullptr;
                memset(&hints, 0, sizeof(hints));
                throw logic_error(string{"getaddrinfo error: "} + gai_strerror(status));
            }

            sock_fd = -1;
            socket_type = SockUnknown;

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
                            ::close(sock_fd);
                            sock_fd = -1;
                        } else {
                            /**
                             * Store the selected peer address
                             */
                            memcpy(&peer_addr, peer->ai_addr, peer->ai_addrlen);
                            peer_len = peer->ai_addrlen;
                            af_type = peer->ai_family;

                            break;
                        }
                    }
                }

                if (sock_fd >= 0)
                    break;
            }

            freeaddrinfo(peer_info);
            peer_info = nullptr;
        }

    };
}

#endif //EZNETWORK_BASIC_SOCKET_H
