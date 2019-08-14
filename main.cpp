#include <iostream>
#include <zconf.h>
#include <wait.h>
#include "src/Node.h"
#include "app/HashTable.h"


int main() {
    static size_t TIMEOUT_IN_SECONDS = 5;

    const auto server = fork();
    if (server == 0) {
        auto servernode = Node();
        servernode.setID(3000);
        while (true) {
            servernode.connectAndReceive(servernode.getID());
        }
    }
    const auto client = fork();
    if (client == 0) {


        auto clientnode = Node();
        clientnode.setID(2000);

        HashTable<bool> h = HashTable<bool>(&clientnode);

        h.insert(4,false);
        std::cout << "bool should be 0: " << h[4] << std::endl;

        std::cout << "bool should be 0: " << h.get(4).value_or("nope") << std::endl;
        std::cout << "size should be 1: " << h.size() << std::endl;
        h.insert(5,true);
        h.insert(6,false);
        std::cout << "size should be 3: " << h.size() << std::endl;

        std::cout << "bool should be 1: " << h[5] << std::endl;
        std::cout << "bool should be 0: " << h[6] << std::endl;
        h.erase(5);
        std::cout << "size should be 2: " << h.size() << std::endl;

        std::cout << "count should be 0: " << h.count(5) << std::endl;
        std::cout << "bool should be not existent: " << h.get(5).has_value() << std::endl;

        auto conn = clientnode.connectClientSocket(3000);
        uint64_t d = reinterpret_cast<uint64_t >("Servus"); // need to cast data to uint64_t
        size_t size = sizeof(d);
        auto f = moderndbs::PosixFile("test", moderndbs::File::WRITE);
        clientnode.sendFile(conn, f);

        std::cout << "Trying to Malloc" << std::endl;
        auto firstgaddr = defs::GlobalAddress(size, nullptr ,0);
        auto recv = clientnode.sendAddress(firstgaddr.sendable(clientnode.getID()), defs::IMMDATA::MALLOC, conn);
        clientnode.closeClientSocket(conn);
        auto test = reinterpret_cast<defs::GlobalAddress *>(recv);
        std::cout << "Got GAddr: " << test->id << ", " << test->size <<", " << test->ptr << std::endl;
        auto data = defs::Data(sizeof(uint64_t), d, *test);
        std::cout << "Trying to Write, data: " << d << std::endl;
        clientnode.write(&data);
        std::cout << "Done. Trying to Read Written Data" << std::endl;
        auto result = clientnode.read(*test);
        std::cout << "Done. Result: ";
        std::cout << reinterpret_cast<char *>(result) << ", and now reading from cache"<<std::endl;
        auto result1 = clientnode.read(*test);
        std::cout << "Done. Result: "<< reinterpret_cast<char *>(result1) << ", and now changing to 1337"<<std::endl;
        auto newint = uint64_t(1337);
        auto newdata = new defs::Data{sizeof(uint64_t), newint, *test};
        clientnode.write(newdata);
        auto result2 = clientnode.read(*test);
        std::cout << "Done. Result: "<< result2 << std::endl;
        std::cout << "Now the first connection is closed. new connection. "<< std::endl;
        std::cout << "Trying to read with the new connection "<< std::endl;
        auto result3 = clientnode.read(*test);
        std::cout << "Done. Result: "<< result3 << ", now we free the memory"<< std::endl;
        clientnode.Free(*test);
        std::cout << "Done freeing. " << std::endl;

    }
    int serverStatus = 1;
    int clientStatus = 1;
    size_t secs = 0;
    for (; secs < TIMEOUT_IN_SECONDS; ++secs, sleep(1)) {
        auto serverTerminated = waitpid(server, &serverStatus, WNOHANG) != 0;
        auto clientTerminated = waitpid(client, &clientStatus, WNOHANG) != 0;
        if (serverTerminated && clientTerminated) {
            break;
        }
    }

    if (secs >= TIMEOUT_IN_SECONDS) {
        std::cerr << "timeout" << std::endl;
        kill(server, SIGTERM);
        kill(client, SIGTERM);
        return 1;
    }
    return serverStatus + clientStatus;

}
