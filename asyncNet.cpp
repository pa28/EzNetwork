//
// Created by hrbuckl on 2/14/18.
//

#include "basic_socket.h"


using namespace async_net;

int main(int argc, char **argv) {

    local_socket localSocket{"", "8000"};

    localSocket.listen(10, AF_INET6);

    return 0;
}