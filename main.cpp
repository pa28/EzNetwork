#include <iostream>
#include <algorithm>
#include "server.h"

using namespace std;
using namespace eznet;

int main() {
    std::cout << "Hello, World!" << std::endl;

    Server server{};

    // Make a socket to bind to any address at port 8000 and add it to the server
    auto serverListen = server.pushBack(make_unique<Socket>("", "8000"));

    // Listen to IPV6 which will also listen to IPV4
    if ((*serverListen)->listen(10, AF_INET6, AF_UNSPEC) < 0) {
        cerr << "Server listen error: " << strerror(errno);
        return 1;
    }

    bool run = true;

    while (run) {
        int s = server.select();
        for (auto first = server.begin(); first != server.end() && s > 0; ++first) {
            if (server.isSelected(first)) {
                --s;
                if (server.isConnectRequest(first)) {
                    int c_fd = server.accept(first);
                } else if (server.isRead(first)) {
                    char buf[BUFSIZ];
                    ssize_t n = ::recv((*first)->fd(), buf, sizeof(buf), 0);
                    if (n > 0)
                        cout.write(buf, n);
                    else
                        (*first)->close();
                }
            }
        }
    }

    return 0;
}