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
    sendLock(fakeLock, defs::RESET);
    socket.close();
}


defs::GlobalAddress *Node::sendAddress(void *data, defs::IMMDATA immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(data, sizeof(data), {});
    auto recvbuf = malloc(sizeof(defs::GlobalAddress));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::GlobalAddress),
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
    return reinterpret_cast<defs::GlobalAddress *>(recvbuf);

}


defs::GlobalAddress *Node::sendData(defs::SendData *data, defs::IMMDATA immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(data, sizeof(defs::SendData), {});
    auto recvbuf = malloc(sizeof(defs::GlobalAddress));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::GlobalAddress),
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
    return reinterpret_cast<defs::GlobalAddress *>(recvbuf);
}

void Node::sendLock(defs::Lock *lock, defs::IMMDATA immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(lock, sizeof(defs::Lock), {});
    auto recvbuf = malloc(sizeof(defs::Lock));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::Lock),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));
    std::cout << "connecting queuepairs" << std::endl;
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    std::cout << "Lock to send: " << lock->id << ", " << lock->state << std::endl;
}

defs::Lock* Node::getLockFromRemote(uint16_t nodeId, defs::IMMDATA immData){
    auto &cq = network.getSharedCompletionQueue();
    auto size = sizeof(defs::Lock);
    auto sendmr = network.registerMr(&nodeId, size, {});
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
    auto result = reinterpret_cast<defs::Lock *>(recvbuf);
    return result;
}