//
// Created by richard on 06/02/18.
//

#ifndef EZNETWORK_SOCKET_H
#define EZNETWORK_SOCKET_H

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
#include "socket_buffer.h"
#include "basic_socket.h"

using namespace std;
using namespace async_net;

namespace eznet {


    /**
     * @brief How sockets should be selected. A bitwise OR mask may be used.
     */
    enum SelectClients {
        SC_None = 0,        ///< No connected sockets are slected
        SC_Read = 1,        ///< Select for read
        SC_Write = 2,       ///< Select for write
        SC_Except = 4,      ///< Select for exception
        SC_All = 7          ///< Select for all
    };


    class Socket : public local_socket {

    public:
        SelectClients selectClients;    ///< How this socket should be selected.

        Socket &operator=(const Socket &) = delete;

        Socket &operator=(Socket &&other) noexcept {
            basic_socket::operator=(std::move(other));
            setStreamBuffer(std::move(other.strmbuf));
            selectClients = other.selectClients;
            sock_future = std::move(other.sock_future);
        }

        unique_ptr<socket_streambuf> strmbuf;  ///< A buffer to abstract the socket as a stream

        iostream    sock_stream;            ///< A stream attached to the socket

    public:
        future<int> sock_future;            ///< Storage for a future returned if asynchronous processing is used.

    protected:

    public:

        /**
         * @brief Create a socket object for a new socket connection.
         */
        explicit Socket(string host,                ///< The hostname or address to connect or bind to
                        string port                 ///< The port number to connect or bind to
        ) : local_socket(host, port),
            selectClients{SC_None},
            sock_stream{nullptr},
            strmbuf{}
        {
        }


        Socket(int fd,
               struct sockaddr *addr,
               socklen_t addr_len
        ) : local_socket(fd, addr, addr_len),
            selectClients{SC_None},
            sock_stream{nullptr},
            strmbuf{}
        {}


        /**
         * @brief Move a unique pointer to a socket_stream into the Socket object
         * @param sbuf an rvalue reference to the socket unique pointer
         * @return true if the strmbuf results in an iostream that is valid
         */
        bool setStreamBuffer(unique_ptr<socket_streambuf> && sbuf) {
            strmbuf = std::move(sbuf);
            sock_stream.rdbuf(strmbuf.get());
            return not sock_stream.bad();
        }


        /**
         * @brief Access to the iostream tied to the underlying socket
         * @return an iostream reference
         */
        std::iostream & iostrm() { return sock_stream; }

    };

}

#endif //EZNETWORK_SOCKET_H
