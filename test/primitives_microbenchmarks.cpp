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
    std::vector<uint8_t> testdata(800);

    int maxThreads = 200;
    for (int threads = 1; threads < maxThreads; ++threads) {

// Change local parameters like num threads
        params.setParam("threads", threads);

// Only print the header for the first iteration
        bool printHeader = threads == 1;

        PerfEventBlock e(1, params, printHeader);
// Counter are started in constructor




        clientnode.Malloc(testdata.size());

// Benchmark counters are automatically stopped and printed on destruction of e
    }
    return 1;
}