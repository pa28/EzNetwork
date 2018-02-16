#include <iostream>
#include <algorithm>
#include <iomanip>
#include "name_that_type.h"
#include "server.h"
#include "iomanip.h"

using namespace std;
using namespace eznet;

/**

   @mainpage EzNetworking Documentation

   - @subpage socket_page_1
   - @subpage server_page_1

 */

/**
   @page socket_page_1 Socket

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

/**

   @page server_page_1 Server

   A _Server_ is a collection of Sockets and the associated management methods. To write a Server
   start by creating a server object:

   @code{.cpp}
   Server server{};
   @endcode

   Next start adding Socket connections. Usually the programmer need only add a small number of
   listening sockets then use the methods included in the Server to add, remove and manage
   the rest.

   @code{.cpp}
   // Make a socket to bind to any address at port 8000 and add it to the server
   auto serverListen = server.push_front(make_unique<Socket>("", "8000"));

   // Listen to IPV6 which will also listen to IPV4
   if ((*serverListen)->listen(10, AF_INET6) < 0) {
      cerr << "Server listen error: " << strerror(errno);
      return 1;
   }

   cout << "Server connection " << (*serverListen)->getPeerName() << endl;
   @endcode

   Now we have a server with a listen socket. A standard select-accept-process loop
   will do the rest:

   @code{.cpp}
   bool run = true;

    while (run) {

        // Perform select call to find Sockets that need service
        int s = server.select();

        // Loop over the Sockets contained in the server and service each one that needs it
        for (auto &&first: server.sockets) {
            if (server.isSelected(first)) {       // Socket needs service
                --s;                              // Decrement count so we may be able to exit for loop early

                // Test to see if the Socket is a listen socket and has a connection request
                if (server.isConnectRequest(first)) {
                    auto newSock = server.accept(first);    // Accept the connection
                    if ((*newSock)->fd() >= 0) {            // If successfull add a stream buffer to the Socket
                        cout << "New connection " << (*newSock)->getPeerName() << endl;
                        run = (*newSock)->setStreamBuffer(make_unique<socket_streambuf>((*newSock)->fd()));
                    }

                // Otherwise test to see if the Socket has input to process
                } else if (server.isRead(first)) {
                    char buf[BUFSIZ];
                    ssize_t  n = first->iostrm().readsome(buf, sizeof(buf));   // non-blocking read
                    if (n > 0) {
                        cout.write(buf, n);
                    } else {
                        cout << "Client " << first->getPeerName() << " disconnected." << endl;
                        first->close();                                        // close connection
                    }
                }
            }
        }
    }
   @endcode

   This is a very basic example, but it does cover the basics.
 */

int main() {
    std::cout << "Hello, World!" << std::endl;

    Server server{};

    // Make a socket to bind to any address at port 8000 and add it to the server
    auto serverListen = server.push_front(make_unique<Socket>("", "8000"));
    (*serverListen)->selectClients = SC_Read;

    // Listen to IPV6 which will also listen to IPV4
    if ((*serverListen)->listen(10, AF_INET6) < 0) {
        cerr << "Server listen error: " << strerror(errno);
        return 1;
    }

    cout << "Server connection " << (*serverListen)->getPeerName() << endl;

    bool run = true;

    while (run) {
        int s = server.select();
        for (auto &&first: server.sockets) {
            if (server.isSelected(first)) {
                --s;
                if (server.isConnectRequest(first)) {
                    auto newSock = server.accept(first);
                    if ((*newSock)->fd() >= 0) {
                        cout << "New connection " << (*newSock)->getPeerName() << endl;
                        run = (*newSock)->setStreamBuffer(make_unique<socket_streambuf>((*newSock)->fd()));
                        (*newSock)->selectClients = SC_Read;
                    }
                } else if (server.isRead(first)) {
                    char buf[BUFSIZ];
                    ssize_t  n = first->iostrm().readsome(buf, sizeof(buf));
                    if (n > 0) {
                        cout.write(buf, n);
                    } else {
                        cout << "Client " << first->getPeerName() << " disconnected." << endl;
                        first->close();
                    }
                }
            }
        }
    }

    return 0;
}