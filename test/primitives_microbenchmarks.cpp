//
// Created by Magdalena Pröbstl on 2019-08-12.
//
#include <src/Node.h>
#include "PerfEvent.hpp"


int main() {
    auto clientnode = Node();
    clientnode.setID(2000);
// Define some global params
    BenchmarkParameters params;
    params.setParam("name", "Test of Malloc");
    std::vector<uint64_t> testdata(defs::MAX_BLOCK_SIZE / sizeof(uint64_t), 123);
    params.setParam("dataSize", testdata.size());

    int maxThreads = 3000;
    std::vector<defs::GlobalAddress> gaddrs;
    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

// Only print the header for the first iteration
        bool printHeader = threads == 1;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor




        gaddrs.push_back(clientnode.Malloc(testdata.size(), clientnode.getID()));

// Benchmark counters are automatically stopped and printed on destruction of e
    }
    params.setParam("name", "Test of Free");
    params.setParam("dataSize", testdata.size());
    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", 3000-threads);

// Only print the header for the first iteration
        bool printHeader = threads == 1;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor
        clientnode.Free(gaddrs.back(), clientnode.getID());
        gaddrs.pop_back();

// Benchmark counters are automatically stopped and printed on destruction of e
    }


    return 1;
}