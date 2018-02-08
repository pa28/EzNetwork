//
// Created by richard on 06/02/18.
//

#ifndef EZNETWORK_SERVER_H
#define EZNETWORK_SERVER_H

#include <memory>
#include "socket.h"

using namespace std;

namespace eznet {

    /**
     * @brief How connected sockets are selected. A bitwise OR mask may be used.
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
     * @brief An abstraction of a network server.
     */
    class Server {
    public:

        using socket_list_t = std::list<std::unique_ptr<eznet::Socket>>;

        /**
         * @brief (constructor)
         * @param port The server port or service name
         * @param host The host name of the server interface
         */
        explicit Server(const string &port, const string &host = "") :
                rd_set{},
                wr_set{},
                ex_set{}
        {
            listeners.push_back(make_unique<Socket>(host,port));
        }


        /**
         * @brief Call ::listen() on a selected socket
         * @param listener An iterator selecting a listening socket
         * @param backlog The socket listen backlog
         * @param af_family_preference The prefered address family
         * @return The value returned by ::listen()
         */
        int listen(socket_list_t::iterator listener, int backlog, int af_family_preference = AF_INET6) {
            return (*listener)->listen(backlog, af_family_preference);
        }


        /**
         * @brief Call listen on each listener socket, store returned status on the socket
         * @param backlog The socket listen backlog
         * @param af_family_preference The preferred address family
         * @return 0 if all calls to listen returned 0, otherwise 1
         */
        int listen(int backlog, int af_family_preference = AF_INET6) {
            int r = 0;
            for (auto l = listeners.begin(); l != listeners.end(); ++l) {
                (*l)->setStatus(listen(l, backlog, af_family_preference));
                if ((*l)->getStatus())
                    r = 1;
            }
        }


        /**
         * @brief Call select on all current sockets associated with the server
         * @param selectClients What operations to select the clients on
         * @param timeout A timeout value in a timeval struct or nullptr for no timeout
         * @return the value returned from ::select()
         */
        int select(SelectClients selectClients = SC_All,
                   struct timeval *timeout = nullptr) {
            // Clear the fd_sets
            FD_ZERO(&rd_set);
            FD_ZERO(&wr_set);
            FD_ZERO(&ex_set);

            int n = 0;

            // Add server listener sockets
            for ( auto &&l: listeners ) {
                FD_SET(l->fd(), &rd_set);
                n = max(n, l->fd()+1);
            }

            // Add server accepted connections
            for ( auto &&s: sockets ) {
                FD_SET(s->fd(), &rd_set);
                n = max(n, s->fd()+1);
            }

            return ::select( n, &rd_set, &wr_set, &ex_set, timeout);
        }


        /**
         * @brief Accept a connection request on a listener socket, add the accepted connection
         * to the connection list.
         * @param listener An iterator selecting the listener socket
         * @return The value returned from ::accept()
         */
        int accept(socket_list_t::iterator listener) {
            struct sockaddr_storage client_addr{};
            socklen_t length;

            int clientfd = ::accept((*listener)->fd(), (struct sockaddr *) &client_addr, &length);
            if (clientfd >= 0) {
                sockets.push_back(std::make_unique<Socket>(clientfd, (struct sockaddr *)&client_addr, length));
                return clientfd;
            }

            return -1;
        }


        /**
         * @brief Determin if a listener socket has been selected due to a connection request
         * @param listener An iterator selecting the listener socket
         * @return true if the listener has a connection request
         */
        bool isConnectRequest(socket_list_t::iterator &listener) {
                return FD_ISSET((*listener)->fd(), &rd_set);
        }


        /**
         * @brief Determin if a socket is selected for read
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isRead(socket_list_t::iterator c) { return FD_ISSET((*c)->fd(), &rd_set); }

        /**
         * @brief Determin if a socket is selected for write
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isWrite(socket_list_t::iterator c) { return FD_ISSET((*c)->fd(), &wr_set); }

        /**
         * @brief Determin if a socket is selected for exception
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isExcept(socket_list_t::iterator c) { return FD_ISSET((*c)->fd(), &ex_set); }


        auto begin() { return sockets.begin(); }                        ///< first connected socket iterator
        auto end() { return sockets.end(); }                            ///< last connected socket iterator

        auto cbegin() const { return sockets.cbegin(); }                ///< const first connected socket iterator
        auto cend() const { return sockets.cend(); }                    ///< const last connected socket iterator

        auto size() const { return sockets.size(); }                    ///< the number of connected sockets
        auto empty() const { return sockets.empty(); }                  ///< true if there are no connected sockets

        auto listenersBegin() { return listeners.begin(); }             ///< first listener socket iterator
        auto listenersEnd() { return listeners.end(); }                 ///< last connected socket iterator

        auto clistenersBegin() const { return listeners.cbegin(); }     ///< const first listener socket iterator
        auto clistenersEnd() const { return listeners.cend(); }         ///< const last connected socket iterator

    protected:
        socket_list_t listeners;        ///< A list of server listening sockets
        socket_list_t sockets;          ///< A list of accepted connection sockets
        fd_set  rd_set, wr_set, ex_set; ///< The file descriptor sets for the select call

    };

}
#endif //EZNETWORK_SERVER_H
