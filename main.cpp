#include <iostream>
#include <zconf.h>
#include <wait.h>

#include "src/Node.h"
#include "app/HashTable.h"


struct __attribute__ ((packed)) Test {
    uint16_t value;
    uint64_t data;
};

/*int main() {
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

        auto f = MaFile("test", moderndbs::File::READ);
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

}*/

int main() {
    auto node = Node();

    std::cout << "Server or Client? (0 = server, 1 = client): ";
    uint16_t servOcli; // 0 = server, 1 = client
    std::cin >> servOcli;

    if (servOcli == 0) {
        node.setID(3000);
        while (true) {
            node.connectAndReceive(node.getID());
        }
    } else if (servOcli == 1) {
        node.setID(2000);
            HashTable<bool> h = HashTable<bool>(&node);

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

        for(auto i = h.begin(); i != h.end(); i++){
            std::cout << "hallo" << std::endl;

        }

        HashTable<Test> ht = HashTable<Test>(&node);
        ht.insert(1,  Test{5, 22323});
        ht.insert(2,  Test{7,303209});
        std::cout << "size should be 2: " << ht.size() << std::endl;

        std::cout << "data 1: " << ht[1].value << ", " << ht[1].data << std::endl;
        std::cout << "data 2: " << ht[2].value << ", " << ht[2].data << std::endl;
        auto res1 = ht[1];
        auto res2 = ht[2];
        std::cout << "data 1: " << res1.value << ", " << res1.data << std::endl;
        std::cout << "data 2: " << res2.value << ", " << res2.data << std::endl;
/*
        uint64_t d = reinterpret_cast<uint64_t >("Servus"); // need to cast data to uint64_t
        size_t size = sizeof(d);
        auto firstgaddr = defs::GlobalAddress(size, nullptr, 0, 0);

        std::cout << "Trying to Malloc" << std::endl;
        auto conn = node.connectClientSocket(3000);
        auto recv = malloc(sizeof(defs::GlobalAddress));
        node.sendAddress(firstgaddr.sendable(node.getID()), defs::IMMDATA::MALLOC,conn,
                                     recv);
        node.closeClientSocket(conn);

        auto test = reinterpret_cast<defs::GlobalAddress *>(recv);

        std::cout << "Got first GAddr: " << test->id << ", " << test->size << ", " << test->ptr
                  << std::endl;

/*
        std::cout << "Trying to Malloc" << std::endl;

        auto test2 = node.Malloc(size, node.getID());

        std::cout << "Got second GAddr: " << test2.id << ", " << test2.size << ", " << test2.ptr
                  << std::endl;

        auto data = defs::Data(sizeof(uint64_t), d, *test);

        std::cout << "Trying to Write, data: " << d << std::endl;
        node.write(&data);

        std::cout << "Done. Trying to Read Written Data" << std::endl;
        auto result = node.read(*test);
        std::cout << "Done. Result: ";
        std::cout << reinterpret_cast<char *>(result) << ", and now reading from cache"
                  << std::endl;

        auto result1 = node.read(*test);
        std::cout << "Done. Result: " << reinterpret_cast<char *>(result1)
                  << ", and now changing to 1337" << std::endl;
        auto newint = uint64_t(1337);
        auto newdata = new defs::Data{sizeof(uint64_t), newint, *test};
        node.write(newdata);

        std::cout << "Done. Trying to Read Written Data" << std::endl;
        auto result2 = node.read(*test);
        std::cout << "Done. Result: " << result2 << ", now we free the memory" << std::endl;

        node.Free(*test);
        std::cout << "freed one" << std::endl;

        node.Free(test2);

        std::cout << "Done freeing. " << std::endl;

//        std::cout << "Trying to malloc File" << std::endl;
//        auto conn2 = node.connectClientSocket(3000);
//        auto fa = defs::GlobalAddress{30, nullptr, 0, true};
//        auto filea = node.sendAddress(fa.sendable(node.getID()), defs::IMMDATA::MALLOCFILE, conn2);
//        node.closeClientSocket(conn2);
//        auto filesga = reinterpret_cast<defs::SendGlobalAddr *>(filea);
        std::cout << "done malloc, gonna write to file" << std::endl;
        char *filename = node.getNextFileName();
        auto fileaddress = defs::GlobalAddress(900, filename, node.getID(), true);
        auto f = MaFile("test", moderndbs::File::READ);
        std::vector<char> block;
        block.resize(900);
        char *readed = &block[0];
        f.read_block(0, 900, readed);
        node.FprintF(readed, fileaddress, 900, 0);

*/

        std::cout << "done, going to save a struct: " << std::endl;
        auto test3 = node.Malloc(sizeof(Test), node.getID());
        auto teststruct = Test{4, 2121};
        auto structdata = defs::Data(sizeof(Test), reinterpret_cast<uintptr_t >(&teststruct),
                                         test3);

        node.write(structdata);


        std::cout << "Done. Trying to Read Written Data" << std::endl;
        auto structres = node.read(test3);
        std::cout << "Done. Result: ";
        auto casstructres = reinterpret_cast<Test*>(structres);
        std::cout << casstructres->value << ", " << casstructres->data << std::endl;


    } else {
        std::cout << "This was no valid Number!" << std::endl;
    }
    return 0;
}
