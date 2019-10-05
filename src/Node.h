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
#include "../util/Lock.h"
#include "Cache.h"
#include "Connection.h"
#include "../util/socket/tcp.h"
#include "../buffermanager/MaFile.h"
#include "../util/RDMANetworking.h"
#include "../rdma/CompletionQueuePair.hpp"


class Node {
private:
    class ScopedLock {
    public:
        ScopedLock(defs::GlobalAddress &gaddr, Node &node, LOCK_STATES ls);

        ~ScopedLock();

    private:
        defs::GlobalAddress &addr;
        Node &node;
    };


    size_t allocated = 0;
    rdma::Network network;
    uint16_t id;
    std::unordered_map<uint64_t, Lock> locks;
    Cache cache;
    Connection c;

    uint16_t filenamesnbr = 0;


    void handleLocks(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd);

    void handleAllocation(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd);

    void handleMallocFile(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd);

    void handleFree(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd);

    void handleRead(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd);

    void handleReadFile(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd);

    bool handleWrite(rdma::CompletionQueuePair &cq,  defs::ReadFileData rfd);

    void handleWriteFile(rdma::CompletionQueuePair &cq, defs::ReadFileData rfd);

    void handleInvalidation(rdma::CompletionQueuePair &cq,  defs::ReadFileData rfd);

    void handleReset(rdma::CompletionQueuePair &cq);

    void handleFile(rdma::CompletionQueuePair &cq,defs::ReadFileData rfd);

    bool sendLock(Lock lock, defs::IMMDATA immData);


    defs::GlobalAddress performWrite(defs::Data data, uint16_t srcID);

    char *performRead(defs::GlobalAddress gaddr, uint16_t srcID);

    void prepareForInvalidate();

    void startInvalidations(defs::Data data, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair &cq, std::vector<uint16_t> nodes,
                            uint16_t srcID);


    void broadcastInvalidations(std::vector<uint16_t> &nodes, defs::GlobalAddress gaddr);

    void sendFile(MaFile &file);

    void sendReadFile(defs::ReadFileData data, defs::IMMDATA immData, char *block);

    defs::GlobalAddress
    sendWriteFile(defs::ReadFileData data, defs::IMMDATA immData, char *block);


public:

    explicit Node();

    char *getNextFileName();

    void connectClientSocket(uint16_t port);

    void closeClientSocket();

    char *sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData);

    defs::SendGlobalAddr sendData(defs::SendingData data, defs::IMMDATA immData);

    bool setLock(uint64_t lockId, LOCK_STATES state, uint16_t nodeId);

    void connectAndReceive(uint16_t port);

    bool receive(Connection &c, rdma::CompletionQueuePair &cq);

    defs::GlobalAddress Malloc(size_t size, uint16_t srcID);

    defs::GlobalAddress Free(defs::GlobalAddress gaddr, uint16_t srcID);

    defs::GlobalAddress write(defs::Data data);

    defs::GlobalAddress FprintF(char *data, defs::GlobalAddress gaddr, size_t size, size_t offset);

    void FreadF(defs::GlobalAddress gaddr, size_t size, size_t offset, char *res);

    char *read(defs::GlobalAddress gaddr);

    bool isLocal(defs::GlobalAddress gaddr);

    inline uint16_t getID() { return id; }

    inline void setID(uint16_t newID) { id = newID; }

};


#endif //MEDMM_NODE_H
