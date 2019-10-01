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
    params.setParam("dataSize", "16 GB");
    std::vector<uint8_t> testdata(defs::MAX_BLOCK_SIZE / sizeof(uint64_t), 123);
    int maxThreads = 3000000;
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
    }/*
    params.setParam("name", "Test of Write");
    params.setParam("dataSize", "16 GB");
    int threads1 = 1;
    for (auto &ga: gaddrs) {

// Change local parameters like num threads
        params.setParam("threads", threads1);

// Only print the header for the first iteration
        bool printHeader = threads1 == 1;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor
        defs::Data d{defs::MAX_BLOCK_SIZE, reinterpret_cast<char *>(testdata.data()),ga};

        clientnode.write(d);
        threads1++;
// Benchmark counters are automatically stopped and printed on destruction of e
    }*/
    params.setParam("name", "Test of Free");
    params.setParam("dataSize", "16 GB");
    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

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