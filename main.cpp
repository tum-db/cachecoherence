#include <iostream>
#include "rdma/Network.hpp"
#include "src/Node.h"


int main() {

    auto node = Node();
    auto test = node.Malloc(20);
    auto res = getNodeId(test);
    std::cout << res << std::endl;
    //setup client or server
    std::cout << "Server or Client? (0 = server, 1 = client): ";
    uint16_t servOcli; // 0 = server, 1 = client
    std::cin >> servOcli;

    if (servOcli == 0) {
        auto result = node.receive();
        auto *sp = static_cast<std::string*>(result);

        std::cout << sp << std::endl;
    } else if (servOcli == 1) {
        std::cout << "Enter data: ";
        std::string input;
        std::cin >> input;
        void* data = &input;
        node.send(data, input.size());
    }
    else {
        std::cout << "This was no valid Number!" << std::endl;
    }
}