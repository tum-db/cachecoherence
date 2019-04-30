#include <iostream>
#include "rdma/Network.hpp"
#include "src/Node.h"

int main() {
    char ip[] = "127.0.0.1";
    uint16_t port = 3000;

    auto net = rdma::Network();
    auto node = Node(net);
    auto gaddr = GlobalAddress(port,ip);
    //setup client or server
    std::cout << "Server or Client? (0 = server, 1 = client): ";
    uint16_t servOcli; // 0 = server, 1 = client
    std::cin >> servOcli;

    if (servOcli == 0) {
        auto result = node.receive(net, gaddr);
        for (auto i = (result.begin()); i != result.end(); ++i)
            std::cout << *i;
    } else if (servOcli == 1) {
        std::cout << "Enter data: ";
        std::string data;
        std::cin >> data;
        node.send(net, data,gaddr);
    }
    else {
        std::cout << "This was no valid Number!" << std::endl;
    }
}