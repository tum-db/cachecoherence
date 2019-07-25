#include <iostream>
#include "rdma/Network.hpp"
#include "src/Node.h"


int main() {
    auto node = Node();

    std::cout << "Server or Client? (0 = server, 1 = client): ";
    uint16_t servOcli; // 0 = server, 1 = client
    std::cin >> servOcli;

    if (servOcli == 0) {
        node.setID(3000);
        while(true) {
            node.connectAndReceive(node.getID());
        }
    } else if (servOcli == 1) {
        node.setID(2000);
        auto conn = node.connectClientSocket(3000);
        uint64_t d = reinterpret_cast<uint64_t >("hallo"); // need to cast data to uint64_t
        size_t size = sizeof(d);
        std::cout << "Trying to Malloc" << std::endl;
        auto firstgaddr = defs::GlobalAddress(size, nullptr ,0);
        auto recv = node.sendAddress(firstgaddr.sendable(node.getID()), defs::IMMDATA::MALLOC, conn);
        node.closeClientSocket(conn);
        auto test = reinterpret_cast<defs::GlobalAddress *>(recv);
        std::cout << "Got GAddr: " << test->id << ", " << test->size <<", " << test->ptr << std::endl;
        auto data = defs::Data(sizeof(uint64_t), d, *test);
        std::cout << "Trying to Write, data: " << d << std::endl;
        node.write(&data);
        std::cout << "Done. Trying to Read Written Data" << std::endl;
        auto result = node.read(*test);
        std::cout << "Done. Result: "<< reinterpret_cast<char *>(result) << ", and now reading from cache"<<std::endl;
        auto result1 = node.read(*test);
        std::cout << "Done. Result: "<< reinterpret_cast<char *>(result1) << ", and now changing to 1337"<<std::endl;
        auto newint = uint64_t(1337);
        auto newdata = new defs::Data{sizeof(uint64_t), newint, *test};
        node.write(newdata);
        auto result2 = node.read(*test);
        std::cout << "Done. Result: "<< result2 << std::endl;
        std::cout << "Now the first connection is closed. new connection. "<< std::endl;
        std::cout << "Trying to read with the new connection "<< std::endl;
        auto result3 = node.read(*test);
        std::cout << "Done. Result: "<< result3 << ", now we free the memory"<< std::endl;
        node.Free(*test);
        std::cout << "Done freeing. " << std::endl;
    } else {
        std::cout << "This was no valid Number!" << std::endl;
    }
}
