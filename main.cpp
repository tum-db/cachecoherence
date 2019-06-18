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
        auto test = node.sendAddress(&size, defs::IMMDATA::MALLOC);
        std::cout << "Got GAddr: " << test->id << ", " << test->size <<", " << test->ptr << std::endl;
        auto data = new defs::SendData{sizeof(uint64_t), d, *test};
        std::cout << "Trying to Write" << std::endl;
        node.write(data);
        std::cout << "Done. Trying to Read Written Data" << std::endl;
        auto result = node.read(test);
        auto number = reinterpret_cast<uint64_t *>(result);
        std::cout << "Done. Result: "<<*number << std::endl;
        node.Free(test);
        node.closeClientSocket();
    } else {
        std::cout << "This was no valid Number!" << std::endl;
    }
}