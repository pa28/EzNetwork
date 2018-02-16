//
// Created by hrbuckl on 2/14/18.
//

#include <future>
#include <atomic>
#include "basic_socket.h"

using namespace std;
using namespace async_net;

class AsyncClient : public basic_socket
{
public:

    AsyncClient() = delete;

    AsyncClient(int fd,                     ///< The accepted connection file descriptor
                 struct sockaddr *addr,      ///< The peer address
                 socklen_t len               ///< The size of the peer address
    ) : basic_socket (fd, addr, len)
    {}


};

class AsyncServer : public local_socket
{
public:
    AsyncServer() = delete;

    AsyncServer(const string &host, const string &port) :
            local_socket(host, port),
            run_server{false}
    {}

    future<int> start() {
        run_server = true;
        return async(launch::async, &AsyncServer::run, this);
    }

protected:
    int run() {
        cout << "Server " << this->getPeerName() << " started." << endl;
        while (run_server) {
            int s = select();
            if (s > 0) {
                auto newSock = accept<AsyncClient>();
                if (*newSock) {
                    // ToDo: Hand the new socket off to some sort of manager.
                    cout << "Connection from " << newSock->getPeerName() << endl;
                    run_server = false;
                }
            }
        }
        return 0;
    }

    atomic_bool run_server;
};


int main(int argc, char **argv) {

    AsyncServer asyncServer{"", "8000"};
    asyncServer.listen(10, AF_INET6);
    future<int> f = asyncServer.start();

    cout << f.get() << endl;

    return 0;
}