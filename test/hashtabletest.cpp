
//
// Created by Magdalena Pröbstl on 2019-08-12.
//

#include <cstddef>
#include <iostream>
#include <src/Node.h>
#include <zconf.h>
#include <wait.h>
#include "app/HashTable.h"


static size_t MESSAGES = 4 * 1024; // ~ 1s
static size_t TIMEOUT_IN_SECONDS = 5;
struct __attribute__ ((packed)) Test {
    uint16_t value;
    uint64_t data;
};
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
        HashTable<bool> h = HashTable<bool>(&clientnode);

        h.insert(4, new bool(false));
        std::cout << "bool should be 0: " << h[4] << std::endl;

        std::cout << "bool should be 0: " << h.get(4).value_or("nope") << std::endl;
        std::cout << "size should be 1: " << h.size() << std::endl;
        h.insert(5, new bool(false));
        h.insert(6, new bool(false));
        std::cout << "size should be 3: " << h.size() << std::endl;

        std::cout << "bool should be 1: " << h[5] << std::endl;
        std::cout << "bool should be 0: " << h[6] << std::endl;
        h.erase(5);
        std::cout << "size should be 2: " << h.size() << std::endl;

        std::cout << "count should be 0: " << h.count(5) << std::endl;
        std::cout << "bool should be not existent: " << h.get(5).has_value() << std::endl;

        for(auto v: h){
            std::cout << "value: " << *v << std::endl;
        }

        h.insert(35, new bool(true));
        h.insert(36, new bool(true));
        h.insert(37, new bool(true));
        h.insert(38, new bool(true));
        h.insert(39, new bool(true));
        h.insert(40, new bool(true));
        h.insert(41, new bool(true));


        h.insert(30, new bool(true));
        std::cout << "size should be 7: " << h.size() << std::endl;
        std::cout << "bool should be 1: " << h[30] << std::endl;

        for(auto i = h.begin(); i != h.end(); i++){
            std::cout << "hallo" << std::endl;

        }

        HashTable<Test> ht = HashTable<Test>(&clientnode);
        ht.insert(1,  Test{5, 22323});
        ht.insert(2, Test{7,303209});
        std::cout << "size should be 2: " << ht.size() << std::endl;

        std::cout << "data 1: " << ht[1].value << ", " << ht[1].data << std::endl;
        std::cout << "data 2: " << ht[2].value << ", " << ht[2].data << std::endl;
        auto res1 = ht[1];
        auto res2 = ht[2];
        std::cout << "data 1: " << res1.value << ", " << res1.data << std::endl;
        std::cout << "data 2: " << res2.value << ", " << res2.data << std::endl;




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