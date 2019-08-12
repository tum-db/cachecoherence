
//
// Created by Magdalena Pröbstl on 2019-08-12.
//

#include <cstddef>
#include <iostream>
#include <src/Node.h>
#include <zconf.h>
#include "app/HashTable.h"


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
        HashTable<bool> h = HashTable<bool>(&clientnode);

        h.insert(4, false);
        std::cout << "bool should be 0: " << h[4] << std::endl;

        std::cout << "bool should be 0: " << h.get(4).value_or("nope") << std::endl;
        std::cout << "size should be 1: " << h.size() << std::endl;
        h.insert(5, true);
        h.insert(6, false);
        std::cout << "size should be 3: " << h.size() << std::endl;

        std::cout << "bool should be 1: " << h[5] << std::endl;
        std::cout << "bool should be 0: " << h[6] << std::endl;
        h.erase(5);
        std::cout << "size should be 2: " << h.size() << std::endl;

        std::cout << "count should be 0: " << h.count(5) << std::endl;
        std::cout << "bool should be not existent: " << h.get(5).has_value() << std::endl;

    }
}