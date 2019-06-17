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
    sendLock(fakeLock, 6);
    socket.close();
}


defs::GlobalAddress *Node::sendAddress(void *data, size_t size, uint32_t immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(data, size, {});
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
    std::cout << "hello 3" << std::endl;
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    rcqp.postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto recvImmData = wc.getImmData();
    std::cout << "got this immdata: " << recvImmData << std::endl;
    return reinterpret_cast<defs::GlobalAddress *>(recvbuf);

}


defs::GlobalAddress *Node::sendData(defs::SendData *data, uint32_t immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto recvbuf = malloc(data->size);
    auto recvmr = network.registerMr(recvbuf, data->size,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto buf = std::vector<uint16_t>{};
    defs::SendData d;
    d.size = data->size;
    buf.resize(data->size + sizeof(defs::SendData));
    memcpy(&buf[0], &d, sizeof(defs::SendData));
    memcpy(&buf[sizeof(defs::SendData)], data, data->size);
    auto sendmr = network.registerMr(buf.data(), data->size, {});

    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));
    std::cout << "connecting queuepairs" << std::endl;

    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteMr, immData);
    rcqp.postWorkRequest(write);
    cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    rcqp.postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto recvImmData = wc.getImmData();
    std::cout << "got this immdata: " << recvImmData << std::endl;
    return reinterpret_cast<defs::GlobalAddress *>(recvbuf);
}

void Node::sendLock(defs::Lock *lock, uint32_t immData) {
    auto &cq = network.getSharedCompletionQueue();
    auto sendmr = network.registerMr(lock, sizeof(defs::Lock), {});
    auto recvbuf = malloc(sizeof(defs::Lock));
    auto recvmr = network.registerMr(recvbuf, sizeof(defs::Lock),
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    std::cout << "hello" << std::endl;
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