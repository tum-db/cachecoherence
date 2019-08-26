//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H


#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"
#include "../util/defs.h"
#include "Cache.h"
#include "Connection.h"
#include "../buffermanager/MaFile.h"
#include <cstddef>
#include <unordered_map>


class Node {
private:
    rdma::Network network;
    uint16_t id;
    std::unordered_map<uint16_t, defs::LOCK_STATES> locks;
    Cache cache;

    uint16_t filenamesnbr = 0;
    char * getNextFileName();

    void handleLocks(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                     rdma::CompletionQueuePair &cq, Connection &c);

    void handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair &cq, Connection &c);

    void handleMallocFile(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair &cq, Connection &c);

    void handleFree(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair &cq, Connection &c);

    void handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair &cq, Connection &c);

    bool handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress
    remoteAddr, rdma::CompletionQueuePair &cq, Connection &c);

    void handleInvalidation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair &cq, Connection &c);

    void handleReset(ibv::memoryregion::RemoteAddress remoteAddr, rdma::CompletionQueuePair &cq,
                     Connection &c);

    void handleFile(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair &cq, Connection &c);

    bool sendLock(defs::Lock lock, defs::IMMDATA immData, Connection &c);


    defs::GlobalAddress performWrite(defs::Data *data, uint16_t srcID);

    defs::SaveData *performRead(defs::GlobalAddress gaddr, uint16_t srcID);

    void prepareForInvalidate(rdma::CompletionQueuePair &cq, Connection &c);

    void startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair &cq, std::vector<uint16_t> nodes,
                            uint16_t srcID, Connection &c);


    void broadcastInvalidations(std::vector<uint16_t> nodes, defs::GlobalAddress gaddr);

    void sendFile(Connection &c, MaFile &file);

public:

    explicit Node();

    Connection connectClientSocket(uint16_t port);

    void closeClientSocket(Connection &c);

    void *sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData, Connection &c);

    defs::GlobalAddress sendData(defs::SendingData data, defs::IMMDATA immData, Connection &c);

    bool setLock(uint16_t lockId, defs::LOCK_STATES state);

    void connectAndReceive(uint16_t port);

    bool receive(Connection &c);

    defs::GlobalAddress Malloc(size_t size, uint16_t srcID);

    defs::GlobalAddress Free(defs::GlobalAddress gaddr);

    defs::GlobalAddress write(defs::Data *data);

    void FprintF(char * data, defs::GlobalAddress gaddr, size_t);

    char * FreadF(defs::GlobalAddress gaddr, size_t size, size_t offset);

    uint64_t read(defs::GlobalAddress gaddr);

    bool isLocal(defs::GlobalAddress gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) { id = newID; }

};


#endif //MEDMM_NODE_H
