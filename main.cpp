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
        node.receive();

    } else if (servOcli == 1) {
        node.setID(15);
        auto size = new size_t(20);
        auto test = node.sendAddress(size, *size, 1);
        auto res = getNodeId(test);
        std::cout << res << std::endl;
        std::cout << node.getID() << std::endl;
        node.Free(test);
    }
    else {
        std::cout << "This was no valid Number!" << std::endl;
    }
}