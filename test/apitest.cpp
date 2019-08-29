//
// Created by Magdalena Pröbstl on 2019-08-12.
//

#include <cstddef>
#include <iostream>
#include <src/Node.h>
#include <zconf.h>
#include <wait.h>


static size_t MESSAGES = 4 * 1024; // ~ 1s
static size_t TIMEOUT_IN_SECONDS = 5;

int main(int, const char **args) {
    if (std::string(args[0]).find("hashtabletest") != std::string::npos) {
        MESSAGES *= 32;
        TIMEOUT_IN_SECONDS *= 32;
    }
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
        auto conn = clientnode.connectClientSocket(3000);
        uint64_t d = reinterpret_cast<uint64_t >("Servus"); // need to cast data to uint64_t
        size_t size = sizeof(d);
        std::cout << "Trying to Malloc" << std::endl;
        auto firstgaddr = defs::GlobalAddress(size, nullptr ,0, false);
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