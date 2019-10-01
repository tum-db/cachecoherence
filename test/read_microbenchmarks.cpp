//
// Created by Magdalena Pröbstl on 01.10.19.
//

//
// Created by Magdalena Pröbstl on 01.10.19.
//

#include <src/Node.h>
#include "PerfEvent.hpp"


int main() {
    auto clientnode = Node();
    clientnode.setID(2000);
// Define some global params
    BenchmarkParameters params;
    params.setParam("name", "Test of Local Read");
    std::vector<uint64_t> testdata(defs::MAX_BLOCK_SIZE / sizeof(uint64_t), 123);
    params.setParam("dataSize", testdata.size());

    int maxThreads = 3000;
    auto gaddrlocal = clientnode.Malloc(testdata.size(), clientnode.getID());
    clientnode.connectClientSocket(3000);
    auto recv = clientnode.sendAddress(gaddrlocal.sendable(clientnode.getID()), defs::IMMDATA::MALLOC);
    clientnode.closeClientSocket();

    auto gaddrremote = defs::GlobalAddress(*reinterpret_cast<defs::SendGlobalAddr *>(recv));

    defs::Data d{testdata.size(), reinterpret_cast<char *>(testdata.data()), gaddrlocal};
    clientnode.write(d);

    defs::Data rd{testdata.size(), reinterpret_cast<char *>(testdata.data()), gaddrremote};
    clientnode.write(rd);

    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

// Only print the header for the first iteration
        bool printHeader = threads == 1;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor

        clientnode.read(gaddrlocal);

// Benchmark counters are automatically stopped and printed on destruction of e
    }

    params.setParam("name", "Test of Remote Read");
    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

// Only print the header for the first iteration
        bool printHeader = false;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor

        clientnode.read(gaddrremote);

// Benchmark counters are automatically stopped and printed on destruction of e
    }

    clientnode.Free(gaddrlocal, clientnode.getID());
    clientnode.Free(gaddrremote, clientnode.getID());


    return 1;
}