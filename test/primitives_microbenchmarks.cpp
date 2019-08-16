//
// Created by Magdalena Pröbstl on 2019-08-12.
//
#include <src/Node.h>
#include "PerfEvent.hpp"

void yourBenchmark(){
    std::vector<uint8_t> testdata(1024);
    auto clientnode = Node();
    clientnode.setID(2000);
    auto conn = clientnode.connectClientSocket(3000);
    uint64_t d = reinterpret_cast<uint64_t >("Servus"); // need to cast data to uint64_t
    size_t size = sizeof(d);
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

int main() {

// Define some global params
    BenchmarkParameters params;
    params.setParam("name", "Test of Malloc");
    params.setParam("dataSize", "16 GB");

    int maxThreads = 3;
    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

// Only print the header for the first iteration
        bool printHeader = threads == 1;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor


        yourBenchmark();

// Benchmark counters are automatically stopped and printed on destruction of e
    }
    return 1;
}