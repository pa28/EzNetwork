//
// Created by richard on 06/02/18.
//

#ifndef EZNETWORK_SERVER_H
#define EZNETWORK_SERVER_H

#include <memory>
#include "socket.h"

using namespace std;

namespace eznet {

    using socket_list_t = std::list<std::unique_ptr<eznet::Socket>>;

    class FD_Set {
    protected:
        int n;                          ///< The largest file descriptor set + 1;

        fd_set rd_set,                  ///< The file descriptor sets for the select call read
                wr_set,                 ///< The file descriptor sets for the select call write
                ex_set;                 ///< The file descriptor sets for the select call exception

    public:
        FD_Set() : n{0}, rd_set{}, wr_set{}, ex_set{} {
            clear();
        }

        void clear() {
            // Clear the fd_sets
            FD_ZERO(&rd_set);
            FD_ZERO(&wr_set);
            FD_ZERO(&ex_set);

            n = 0;
        }

        void set(const socket_list_t::iterator sock) {
            set(*sock);
        }

        void set(unique_ptr<Socket> &sock) {
            if (sock->selectClients != SC_None) {
                if (sock->selectClients & SC_Read)
                    FD_SET(sock->fd(), &rd_set);
                if (sock->selectClients & SC_Write)
                    FD_SET(sock->fd(), &wr_set);
                if (sock->selectClients & SC_Except)
                    FD_SET(sock->fd(), &ex_set);
                n = max(n, sock->fd() + 1);
            }
        }

        int select(struct timeval *timeout = nullptr) {
            return ::select(n, &rd_set, &wr_set, &ex_set, timeout);
        }

        bool isRead(unique_ptr<Socket> &s) { return FD_ISSET((*s).fd(), &rd_set); }

        bool isWrite(unique_ptr<Socket> &s) { return FD_ISSET((*s).fd(), &wr_set); }

        bool isExcept(unique_ptr<Socket> &s) { return FD_ISSET((*s).fd(), &ex_set); }

        bool isSelected(unique_ptr<Socket> &s) { return isRead(s) || isWrite(s) || isExcept(s); }

    };

    /**
     * @brief An abstraction of a network server.
     */
    class Server {
    public:

        string errorString;

        /**
         * @brief (constructor)
         */
        Server() = default;


        /**
         * @brief Call select on all current sockets associated with the server
         * @param selectClients What operations to select the clients on
         * @param timeout A timeout value in a timeval struct or nullptr for no timeout
         * @return the value returned from ::select()
         */
        int select(SelectClients selectClients = SC_All,
                   struct timeval *timeout = nullptr) {
            // Move new sockets onto the list.
            for (auto &&ns: newSockets) {
                sockets.push_back(std::move(ns));
            }

            // Clean sockets
            auto socket = sockets.begin();
            while (socket != sockets.end()) {
                if ((*socket)->fd() < 0) {
                    socket = sockets.erase(socket);
                } else {
                    ++socket;
                }
            }

            newSockets.clear();

            fd_set.clear();

            // Select all sockets
            for (auto &&s: sockets) {
                fd_set.set(s);
            }

            return fd_set.select(timeout);
        }


        /**
         * @brief Accept a connection request on a listener socket, add the accepted connection
         * to the connection list.
         * @param listener An iterator selecting the listener socket
         * @return An iterator pointing to the created Socket
         */
        auto accept(socket_list_t::iterator &listener) {
            if ((*listener)->socketType() == SockListen) {
                struct sockaddr_storage client_addr{};
                socklen_t length = sizeof(client_addr);

                int clientfd = ::accept((*listener)->fd(), (struct sockaddr *) &client_addr, &length);
                newSockets.push_back(std::make_unique<Socket>(clientfd, (struct sockaddr *) &client_addr, length));
                return newSockets.rbegin();
            }

            throw logic_error("Accept on a non-listening socket.");
        }


        /**
         * @brief Determin if a listener socket has been selected due to a connection request
         * @param listener An iterator selecting the listener socket
         * @return true if the listener has a connection request
         */
        bool isConnectRequest(socket_list_t::iterator &listener) {
            return listener != sockets.end() &&
                   (*listener)->socketType() == SocketType::SockListen &&
                   fd_set.isRead(*listener);
        }


        /**
         * @brief Determin if a socket is selected for read
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isRead(socket_list_t::iterator &c) { return fd_set.isRead(*c); }


        /**
         * @brief Determin if a socket is selected for write
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isWrite(socket_list_t::iterator &c) { return fd_set.isWrite(*c); }


        /**
         * @brief Determine if a socket is selected for exception
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isExcept(socket_list_t::iterator &c) { return fd_set.isExcept(*c); }


        /**
         * @brief Deterimin if a socket is selected for any state
         * @param listener an iterator selecting a socket
         * @return true if selected
         */
        bool isSelected(socket_list_t::iterator &listener) { return fd_set.isSelected(*listener); }


        auto begin() { return sockets.begin(); }            ///< first connected socket iterator
        auto end() { return sockets.end(); }                ///< last connected socket iterator

        auto cbegin() const { return sockets.cbegin(); }    ///< const first connected socket iterator
        auto cend() const { return sockets.cend(); }        ///< const last connected socket iterator

        auto size() const { return sockets.size(); }        ///< the number of connected sockets
        auto empty() const { return sockets.empty(); }      ///< true if there are no connected sockets

        auto pushBack(unique_ptr<Socket> socketPtr) {       ///< Push back a unique pointer to a Socket
            sockets.push_back(std::move(socketPtr));
            return sockets.rbegin();
        }

    protected:
        socket_list_t sockets;          ///< A list of accepted connection sockets
        socket_list_t newSockets;       ///< A list of sockets accepted
        FD_Set fd_set;                  ///< An object containing the fd_sets used
    };
}

#endif //EZNETWORK_SERVER_H
