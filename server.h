//
// Created by richard on 06/02/18.
//

#ifndef EZNETWORK_SERVER_H
#define EZNETWORK_SERVER_H

#include <memory>
#include "socket.h"

using namespace std;

namespace eznet {

    template <class SocketContainer, class SocketPtr>
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


        /**
         * @brief Clear all file descriptor sets.
         */
        void clear() {
            // Clear the fd_sets
            FD_ZERO(&rd_set);
            FD_ZERO(&wr_set);
            FD_ZERO(&ex_set);

            n = 0;
        }


        /**
         * @brief Given a socket container iterator, set the socket selection criteria
         * @param sock the iterator
         */
        void set(const typename SocketContainer::iterator sock) {
            set(*sock);
        }


        /**
         * Given a socket pointer, set the socket selection criteria
         * @param sock the pointer
         */
        void set(SocketPtr &sock) {
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


        /**
         * @brief Make the select call
         * @param timeout An optional timeout value
         * @return The number of file descriptors selected.
         */
        int select(struct timeval *timeout = nullptr) {
            return ::select(n, &rd_set, &wr_set, &ex_set, timeout);
        }

        bool isRead(SocketPtr &s) { return FD_ISSET((*s).fd(), &rd_set); }      ///< Test for read selection

        bool isWrite(SocketPtr &s) { return FD_ISSET((*s).fd(), &wr_set); }     ///< Test for write selection

        bool isExcept(SocketPtr &s) { return FD_ISSET((*s).fd(), &ex_set); }    ///< Test for exception selection

        bool isSelected(SocketPtr &s) { return isRead(s) || isWrite(s) || isExcept(s); }    ///< Test for any selection

    };


    /**
     * @brief The default server policy class
     * @details A server policy class is used to set library default behavior at compile time.
     *
     * - *acceptFlags* Flags set on all client connections accepted by the server
     * - *push_front*  A method that provides the same semantics across standard library containers for push_front.
     */

    template <class T>
    class DefaultServerPolicy
    {
    public:

        /**
         * @brief Return a bitwise or of the flags to pass as flags to accept4(2)
         * @return the flag mask
         */
        int acceptFlags = SOCK_CLOEXEC;

        using socket_ptr_t = T;
        using socket_container_t = std::list<T>;
        using socket_iterator_t = typename socket_container_t::iterator;


        /**
         * @brief Provide a method to prepend a socket to the socket container, and return an iterator to the new socket.
         * @param sockets The socket container
         * @param socketPtr A pointer to the new socket
         * @return An iterator pointing to the new socket in the container.
         * @details This method is required because not all standard library containers return an iterator from
         * the push_front method.
         */
        socket_iterator_t push_front(socket_container_t &sockets, socket_ptr_t socketPtr) {
            sockets.push_front(std::move(socketPtr));
            return sockets.begin();
        }

        socket_iterator_t erase(socket_container_t &sockets, socket_iterator_t itr) {
            return sockets.erase(itr);
        }
    };

    /**
     * @brief An abstraction of a network server.
     */

    template <class Policy = DefaultServerPolicy<std::unique_ptr<eznet::Socket>>>
    class Server : private Policy {
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
                    socket = Policy::erase(sockets,socket);
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
        auto accept(typename Policy::socket_iterator_t &listener) {
            return accept(*listener);
        }


        /**
         * @brief Accept a connection request on a listener socket, add the accepted connection
         * to the connection list.
         * @param listener A pointer to the listener socket
         * @return An iterator pointing to the created Socket
         */
        auto accept(typename Policy::socket_ptr_t &listener) {
            if (listener->socketType() == SockListen) {
                struct sockaddr_storage client_addr{};
                socklen_t length = sizeof(client_addr);

                int clientfd = ::accept4(listener->fd(), (struct sockaddr *) &client_addr, &length, Policy::acceptFlags);
                newSockets.push_back(std::make_unique<Socket>(clientfd, (struct sockaddr *) &client_addr, length));
                return newSockets.rbegin();
            }

            throw logic_error("Accept on a non-listening socket.");
        }


        /**
         * @brief Determine if a listener socket has been selected due to a connection request
         * @param listener An iterator selecting the listener socket
         * @return true if the listener has a connection request
         */
        bool isConnectRequest(typename Policy::socket_iterator_t &listener) {
            if (listener != sockets.end())
                return isConnectRequest(*listener);
            return false;
        }


        /**
         * @brief Determine if a listener socket has been selected due to a connection request
         * @param listener A pointer to the listener socket
         * @return true if the listener has a connection request
         */
        bool isConnectRequest(typename Policy::socket_ptr_t &listener) {
            return listener->socketType() == SocketType::SockListen &&
                   fd_set.isRead(listener);
        }


        /**
         * @brief Determine if a socket is selected for read
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isRead(typename Policy::socket_iterator_t &c) { return fd_set.isRead(*c); }


        /**
         * @brief Determine if a socket is selected for read
         * @param c a pointer to a socket
         * @return true if selected
         */
        bool isRead(typename Policy::socket_ptr_t &c) { return fd_set.isRead(c); }


        /**
         * @brief Determine if a socket is selected for write
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isWrite(typename Policy::socket_iterator_t &c) { return fd_set.isWrite(*c); }


        /**
         * @brief Determine if a socket is selected for write
         * @param c a pointer to a socket
         * @return true if selected
         */
        bool isWrite(typename Policy::socket_ptr_t &c) { return fd_set.isWrite(c); }


        /**
         * @brief Determine if a socket is selected for exception
         * @param c an iterator selecting a socket
         * @return true if selected
         */
        bool isExcept(typename Policy::socket_iterator_t &c) { return fd_set.isExcept(*c); }


        /**
         * @brief Determine if a socket is selected for exception
         * @param c a pointer to a socket
         * @return true if selected
         */
        bool isExcept(typename Policy::socket_ptr_t &c) { return fd_set.isExcept(c); }


        /**
         * @brief Determine if a socket is selected for any state
         * @param s an iterator selecting a socket
         * @return true if selected
         */
        bool isSelected(typename Policy::socket_iterator_t &s) { return fd_set.isSelected(*s); }


        /**
         * @brief Determine if a socket is selected for any state
         * @param s a pointer to the socket
         * @return
         */
        bool isSelected(typename Policy::socket_ptr_t &s) { return fd_set.isSelected(s); }


        /**
         * @brief Move a new socket into the front of the socket container, the container takes ownership of the socket
         * @param socketPtr A pointer to the socket to move.
         * @return An iterator pointing to the socket pointer in the container
         */
        auto push_front(typename Policy::socket_ptr_t socketPtr) {
            return Policy::push_front(sockets, std::move(socketPtr));
        }

        typename Policy::socket_container_t sockets;          ///< A list of accepted connection sockets

    protected:
        typename Policy::socket_container_t newSockets;       ///< A list of sockets accepted
        FD_Set<typename Policy::socket_container_t, typename Policy::socket_ptr_t> fd_set;   ///< An object containing the fd_sets used
    };
}

#endif //EZNETWORK_SERVER_H
