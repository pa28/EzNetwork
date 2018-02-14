//
// Created by hrbuckl on 2/14/18.
//

#include <iostream>
#include <algorithm>
#include <iomanip>
#include <future>
#include <chrono>
#include "name_that_type.h"
#include "server.h"
#include "iomanip.h"

using namespace std;
using namespace eznet;

int doClient(std::unique_ptr<eznet::Socket>::pointer sock) {
    int c;
    while ((c = sock->iostrm().get()) != EOF) {
        cout.put(c);
    }

    cout << "Client " << sock->getPeerName() << " disconnected." << endl;
    sock->close();

    return 0;
}

int main(int argc, char ** argv) {
    std::cout << "Hello, World!" << std::endl;

    Server server{};

    // Make a socket to bind to any address at port 8000 and add it to the server
    auto serverListen = server.push_front(make_unique<Socket>("", "8000"));

    // Listen to IPV6 which will also listen to IPV4
    if ((*serverListen)->listen(10, AF_INET6) < 0) {
        cerr << "Server listen error: " << strerror(errno);
        return 1;
    }

    cout << "Server connection " << (*serverListen)->getPeerName() << endl;

    bool run = true;

    while (run) {
        chrono::seconds timeOut(10);
        int s = server.select(timeOut);

        time_t now;
        time(&now);

        cerr << "\n" << now << " select => " << s << endl;

        if (s > 0) {
            for (auto &&sock: server.sockets) {
                if (server.isConnectRequest(sock)) {
                    auto newSock = server.accept(sock);
                    if ((*newSock)->fd() >= 0) {
                        cout << "New connection " << (*newSock)->getPeerName() << endl;
                        (*newSock)->selectClients = SC_None;
                        run = (*newSock)->setStreamBuffer(make_unique<socket_streambuf>((*newSock)->fd()));
                        (*newSock)->sock_future = std::async(doClient, (*newSock).get());
                    }
                    --s;
                    if (s == 0)
                        break;
                }
            }
        }
    }

    return 0;
}