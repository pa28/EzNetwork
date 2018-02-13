#include <iostream>
#include <algorithm>
#include <iomanip>
#include "name_that_type.h"
#include "server.h"
#include "iomanip.h"

using namespace std;
using namespace eznet;

int main() {
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
        int s = server.select();
        for (auto first = server.sockets.begin(); first != server.sockets.end() && s > 0; ++first) {
            if (server.isSelected(first)) {
                --s;
                if (server.isConnectRequest(first)) {
                    auto newSock = server.accept(first);
                    if ((*newSock)->fd() >= 0) {
                        cout << "New connection " << (*newSock)->getPeerName() << endl;
                        run = (*newSock)->setStreamBuffer(make_unique<socket_streambuf>((*newSock)->fd()));
                    }
                } else if (server.isRead(first)) {
                    char buf[BUFSIZ];
                    ssize_t  n = (*first)->iostrm().readsome(buf, sizeof(buf));
                    if (n > 0) {
                        cout.write(buf, n);
                    } else {
                        cout << "Client " << (*first)->getPeerName() << " disconnected." << endl;
                        (*first)->close();
                    }
                }
            }
        }
    }

    return 0;
}