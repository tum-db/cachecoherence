//
// Created by Magdalena Pröbstl on 2019-04-11.
//

#ifndef MEDMM_NODE_H
#define MEDMM_NODE_H

#include <unordered_map>
#include <stdlib.h>
#include <cstdio>
#include <thread>
#include <zconf.h>
#include "../util/Locks.h"
#include "Cache.h"
#include "Connection.h"
#include "../util/socket/tcp.h"
#include "../buffermanager/MaFile.h"
#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"


class Node {
private:
    size_t allocated = 0;
    rdma::Network network;
    uint16_t id;
    std::unordered_map<uint64_t, LOCK_STATES> locks;
    Cache cache;


    uint16_t filenamesnbr = 0;


    void handleLocks(ibv::memoryregion::RemoteAddress remoteAddr,
                     rdma::CompletionQueuePair &cq, Connection &c);

    void handleAllocation(ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair &cq, Connection &c);

    void handleMallocFile( ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair &cq, Connection &c);

    void handleFree(ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair &cq, Connection &c);

    void handleRead(ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair &cq, Connection &c);

    void handleReadFile(ibv::memoryregion::RemoteAddress remoteAddr,
                        rdma::CompletionQueuePair &cq, Connection &c);

    bool handleWrite(ibv::memoryregion::RemoteAddress remoteAddr,
                     rdma::CompletionQueuePair &cq, Connection &c);

    void handleWriteFile(ibv::memoryregion::RemoteAddress remoteAddr,
                         rdma::CompletionQueuePair &cq, Connection &c);

    void handleInvalidation(ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair &cq, Connection &c);

    void handleReset(ibv::memoryregion::RemoteAddress remoteAddr, rdma::CompletionQueuePair &cq,
                     Connection &c);

    void handleFile(ibv::memoryregion::RemoteAddress remoteAddr,
                    rdma::CompletionQueuePair &cq, Connection &c);

    void sendLock(Lock lock, defs::IMMDATA immData, Connection &c, bool *result);


    defs::GlobalAddress performWrite(defs::Data *data, uint16_t srcID);

    defs::SaveData *performRead(defs::GlobalAddress gaddr, uint16_t srcID);

    void prepareForInvalidate(rdma::CompletionQueuePair &cq, Connection &c);

    void startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair &cq, std::vector<uint16_t> nodes,
                            uint16_t srcID, Connection &c);


    void broadcastInvalidations(std::vector<uint16_t> nodes, defs::GlobalAddress gaddr);

    void sendFile(Connection &c, MaFile &file);

    void sendReadFile(defs::ReadFileData data, defs::IMMDATA immData, Connection &c, char *block);

    void
    sendWriteFile(defs::ReadFileData data, defs::IMMDATA immData, Connection &c, uint64_t *block,
                  defs::SendGlobalAddr *buffer);


public:

    explicit Node();

    char *getNextFileName();

    Connection connectClientSocket(uint16_t port);

    void closeClientSocket(Connection &c);

    void sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData, Connection &c, void *buffer);

    void sendData(defs::SendingData data, defs::IMMDATA immData, Connection &c,
                  defs::SendGlobalAddr *buffer);

    bool setLock(uint64_t lockId, LOCK_STATES state);

    void connectAndReceive(uint16_t port);

    bool receive(Connection &c, rdma::CompletionQueuePair &cq);

    defs::GlobalAddress Malloc(size_t size, uint16_t srcID);

    defs::GlobalAddress Free(defs::GlobalAddress gaddr);

    defs::GlobalAddress write(defs::Data *data);

    defs::GlobalAddress FprintF(char *data, defs::GlobalAddress gaddr, size_t size, size_t offset);

    char *FreadF(defs::GlobalAddress gaddr, size_t size, size_t offset);

    uint64_t read(defs::GlobalAddress gaddr);

    bool isLocal(defs::GlobalAddress gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) { id = newID; }

};


#endif //MEDMM_NODE_H
