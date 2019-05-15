#include <iostream>
#include <malloc.h>
#include "rdma/Network.hpp"
#include "src/Node.h"


int main() {
    auto node = Node();

    std::cout << "Server or Client? (0 = server, 1 = client): ";
    uint16_t servOcli; // 0 = server, 1 = client
    std::cin >> servOcli;

    if (servOcli == 0) {
        node.setID(10);
        auto result = node.receive();
        std::cout << result->id << std::endl;
    } else if (servOcli == 1) {
        node.setID(15);
        auto size = new size_t(20);
        auto test = node.sendRemoteMalloc(size);
        auto res = getNodeId(test);
        std::cout << res << std::endl;
        std::cout << node.getID() << std::endl;
    }
    else {
        std::cout << "This was no valid Number!" << std::endl;
    }
}