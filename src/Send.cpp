//
// Created by Magdalena Pröbstl on 2019-06-15.
//

#include "Node.h"
#include "../util/defs.h"
#include "../util/socket/tcp.h"


Connection Node::connectClientSocket(uint16_t port) {
    auto c = Connection{rdma::RcQueuePair(network, network.getSharedCompletionQueue()),l5::util::Socket::create()};
    auto remoteAddr = rdma::Address{network.getGID(), c.rcqp.getQPN(), network.getLID()};
    l5::util::tcp::connect(c.socket, defs::ip, port);
    l5::util::tcp::write(c.socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(c.socket, &remoteAddr, sizeof(remoteAddr));
    std::cout << "connecting queuepairs" << std::endl;
    c.rcqp.connect(remoteAddr);
    return c;
}

void Node::closeClientSocket(Connection *c) {
    std::cout << "closing socket" << std::endl;
    auto fakeLock = defs::Lock{id, defs::LOCK_STATES::UNLOCKED};
    sendLock(fakeLock, defs::RESET, c);
    c->rcqp.setToResetState();
    c->socket.close();
}


void *Node::sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData, Connection *c) {
    auto &cq = network.getSharedCompletionQueue();
    auto size = sizeof(defs::SendGlobalAddr) + data.size;
    auto sendmr = network.registerMr(&data, sizeof(data) + data.size, {});
    auto recvbuf = malloc(size);
    auto recvmr = network.registerMr(recvbuf, size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(c->socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c->socket, &remoteMr, sizeof(remoteMr));
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c->rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c->rcqp.postRecvRequest(recv);
    cq.pollRecvWorkCompletionBlocking();
    return recvbuf;

}


defs::GlobalAddress Node::sendData(defs::SendingData data, defs::IMMDATA immData, Connection *c) {
    std::cout << "sendable: " << data.data << std::endl;
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&data, sizeof(defs::SendingData), {});
    auto recvbuf = malloc(sizeof(defs::SendGlobalAddr));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::SendGlobalAddr),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(c->socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c->socket, &remoteMr, sizeof(remoteMr));
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c->rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    c->rcqp.postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto newImmData = wc.getImmData();
    if (newImmData == defs::IMMDATA::INVALIDATE) {
        std::cout << "invalidating cache" << std::endl;
        c->socket.close();
        auto recv2 = ibv::workrequest::Recv{};
        recv2.setSge(nullptr, 0);
        c->rcqp.postRecvRequest(recv2);
        cq.pollRecvWorkCompletionBlocking();
        c->rcqp.setToResetState();
        connectAndReceive(id);
        // invalidation is running
    }
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = defs::GlobalAddress(*sga);
    return gaddr;
}

bool Node::sendLock(defs::Lock lock, defs::IMMDATA immData, Connection *c) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&lock, sizeof(defs::Lock), {});
    auto recvbuf = malloc(sizeof(defs::Lock));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::Lock),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(c->socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(c->socket, &remoteMr, sizeof(remoteMr));
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    c->rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    if (immData != defs::IMMDATA::RESET) {
        auto recv = ibv::workrequest::Recv{};
        recv.setSge(nullptr, 0);
        c->rcqp.postRecvRequest(recv);
        cq.pollRecvWorkCompletionBlocking();
        auto result = reinterpret_cast<bool *>(recvbuf);
        return *result;
    }
    return false;
}


void Node::invalidate(rdma::CompletionQueuePair *cq, Connection *c) {
    auto recv2 = ibv::workrequest::Recv{};
    recv2.setSge(nullptr, 0);
    c->rcqp.postRecvRequest(recv2);
    cq->pollRecvWorkCompletionBlocking();
    c->socket.close();
    // invalidation is running
    connectAndReceive(id);

}

void Node::broadcastInvalidations(std::vector<uint16_t> nodes,
                                  defs::GlobalAddress gaddr) {
    for (const auto &n: nodes) {
        std::cout << "invalidation of node " << n << std::endl;
        auto connection = connectClientSocket(n);
        sendAddress(gaddr.sendable(id), defs::IMMDATA::INVALIDATE, &connection);
        closeClientSocket(&connection);
    }
}