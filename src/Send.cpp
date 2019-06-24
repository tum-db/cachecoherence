//
// Created by Magdalena Pröbstl on 2019-06-15.
//

#include "Node.h"
#include "../util/defs.h"
#include "../util/socket/tcp.h"


void Node::connectClientSocket() {
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    l5::util::tcp::connect(socket, defs::ip, defs::port);
    l5::util::tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(socket, &remoteAddr, sizeof(remoteAddr));
    std::cout << "connecting queuepairs" << std::endl;
    rcqp.connect(remoteAddr);
}

void Node::closeClientSocket() {
    auto fakeLock = new defs::Lock{id, defs::UNSHARED};
    sendLock(*fakeLock, defs::RESET);
    socket.close();
}


void *Node::sendAddress(defs::SendGlobalAddr data, defs::IMMDATA immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto size = sizeof(defs::SendGlobalAddr) + data.size;
    auto sendmr = network.registerMr(&data, sizeof(data) + data.size, {});
    auto recvbuf = malloc(size);
    auto recvmr = network.registerMr(recvbuf, size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    rcqp.postRecvRequest(recv);
    cq.pollRecvWorkCompletionBlocking();
    return recvbuf;

}


defs::GlobalAddress *Node::sendData(defs::SendingData data, defs::IMMDATA immData) {
    std::cout << "sendable: " << data.data << std::endl;

    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&data, sizeof(defs::SendingData), {});
    auto recvbuf = malloc(sizeof(defs::SendGlobalAddr));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::SendGlobalAddr),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    rcqp.postRecvRequest(recv);
    cq.pollRecvWorkCompletionBlocking();
    auto sga = reinterpret_cast<defs::SendGlobalAddr *>(recvbuf);
    auto gaddr = new defs::GlobalAddress(*sga);
    return gaddr;
}

bool Node::sendLock(defs::Lock lock, defs::IMMDATA immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(&lock, sizeof(defs::Lock), {});
    auto recvbuf = malloc(sizeof(defs::Lock));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::Lock),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    if (immData != defs::IMMDATA::RESET) {
        auto recv = ibv::workrequest::Recv{};
        recv.setSge(nullptr, 0);
        rcqp.postRecvRequest(recv);
        cq.pollRecvWorkCompletionBlocking();
        auto result = reinterpret_cast<bool *>(recvbuf);
        return *result;
    }
    return false;
}