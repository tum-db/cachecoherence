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
        node.setID(1);
        node.connectAndReceive();
    } else if (servOcli == 1) {
        node.setID(2);
        node.connectClientSocket();
        uint64_t d = 26121994;
        size_t size = sizeof(d);
        std::cout << "Trying to Malloc" << std::endl;
        auto recv = node.sendAddress(defs::GlobalAddress{size, 0,0}, defs::IMMDATA::MALLOC);
        auto test = reinterpret_cast<defs::GlobalAddress *>(recv);
        std::cout << "Got GAddr: " << test->id << ", " << test->size <<", " << test->ptr << std::endl;
        auto data = new defs::SendData{sizeof(uint64_t), d, *test};
        std::cout << "Trying to Write" << std::endl;
        node.write(data);
        std::cout << "Done. Trying to Read Written Data" << std::endl;
        auto result = node.read(test);
        std::cout << "Done. Result: "<<result << std::endl;
        node.Free(test);
        node.closeClientSocket();
    } else {
        std::cout << "This was no valid Number!" << std::endl;
    }
}