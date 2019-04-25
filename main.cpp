#include <iostream>
#include <cstddef>
#include <libibverbscpp.h>
#include <vector>
#include <thread>
#include <arpa/inet.h>
#include "rdma/Network.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/RcQueuePair.h"
#include "util/RDMANetworking.h"
#include "src/Node.h"
#include "util/socket/tcp.h"
#include "util/socket/domain.h"
#include <netinet/in.h>
#include <chrono>
#include <fstream>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <random>

constexpr uint16_t port = 3000;


void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main() {
    char *ip = "127.0.0.1";

    auto net = rdma::Network();
    auto node = Node(net);
    //setup client or server
    std::cout << "Server or Client? (0 = server, 1 = client): ";
    uint16_t servOcli; // 0 = server, 1 = client
    std::cin >> servOcli;

    if (servOcli == 0) {
        auto result = node.receive(net, port, ip);
        for (auto i = (result.begin()); i != result.end(); ++i)
            std::cout << *i;
    } else if (servOcli == 1) {
        std::cout << "Enter data: ";
        std::string data;
        std::cin >> data;
        node.send(net, data,port,ip);
    }
}