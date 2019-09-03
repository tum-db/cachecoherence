//
// Created by Magdalena Pröbstl on 2019-07-25.
//

#ifndef MEDMM_CONNECTION_H
#define MEDMM_CONNECTION_H


#include <bits/unique_ptr.h>
#include "../rdma/RcQueuePair.h"
#include "../util/socket/Socket.h"

class Connection {
public:
    Connection(Connection &&c) noexcept;

    Connection(std::unique_ptr<rdma::RcQueuePair> uniquePtr, l5::util::Socket s);

    Connection &operator=(Connection &&other) noexcept;

    std::unique_ptr<rdma::RcQueuePair> rcqp;
    l5::util::Socket socket;

    std::unique_ptr<ibv::memoryregion::MemoryRegion> sendmr;
    std::unique_ptr<ibv::memoryregion::MemoryRegion> recvmr;

    char *recvreg;
    char *sendreg;

    ibv::memoryregion::RemoteAddress remoteMr;


};


#endif //MEDMM_CONNECTION_H
