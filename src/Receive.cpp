//
// Created by Magdalena Pröbstl on 2019-06-15.
//


#include "Node.h"
#include "../util/socket/tcp.h"

l5::util::Socket Node::connectServerSocket() {
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    l5::util::tcp::listen(socket);
    auto acced = l5::util::tcp::accept(socket);
    l5::util::tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
    rcqp.connect(remoteAddr);
    return acced;
}

void Node::receive(l5::util::Socket *acced) {
    auto &cq = network.getSharedCompletionQueue();
    auto recvbuf = malloc(defs::BIGBADBUFFER_SIZE * 2);
    auto recvmr = network.registerMr(recvbuf, defs::BIGBADBUFFER_SIZE * 2,
                                     {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf),
                                                     recvmr->getRkey()};
    l5::util::tcp::write(*acced, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(*acced, &remoteMr, sizeof(remoteMr));
    auto recv = ibv::workrequest::Recv{};
    recv.setSge(nullptr, 0);
    rcqp.postRecvRequest(recv);
    auto wc = cq.pollRecvWorkCompletionBlocking();
    auto immData = wc.getImmData();
    std::cout << "got this immdata: " << immData << std::endl;
    switch (immData) {
        case defs::IMMDATA::MALLOC:  //immdata = 1, if it comes from another server
        {
            handleAllocation(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::READ: //immdata = 2, if it is a read
        {
            handleRead(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::FREE://immdata = 3, it should be freed
        {
            handleFree(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::WRITE:  //immdata = 4, write
        {
            handleWrite(recvbuf, remoteMr, &cq);
            break;
        }
        case defs::IMMDATA::LOCKS:  //immdata = 5, save lock
        {
            handleReceivedLocks(recvbuf);
            break;
        }
        case defs::IMMDATA::RESET: {
            rcqp.setToResetState();
            *acced = connectServerSocket();
            break;
        }
        case defs::IMMDATA::GETLOCK: {
            handleGetLocks(recvbuf, remoteMr, &cq);
            break;
        }
        default: {
            return;
        }
    }
}

void Node::connectAndReceive() {
    l5::util::tcp::bind(socket, defs::port);
    auto acced = connectServerSocket();
    while (true) {
        receive(&acced);
    }
}

void Node::handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                            rdma::CompletionQueuePair *cq) {
    auto size = reinterpret_cast<size_t *>(recvbuf);
    auto newgaddr = Malloc(size);
    auto sendmr = network.registerMr(newgaddr, sizeof(defs::GlobalAddress), {});
    std::cout << newgaddr->size << ", " << newgaddr->id << ", " << newgaddr->ptr
              << std::endl;
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleFree(void *recvbuf, ibv::memoryregion::RemoteAddress
remoteAddr, rdma::CompletionQueuePair *cq) {
    auto gaddr = reinterpret_cast<defs::GlobalAddress *>(recvbuf);
    auto res = Free(gaddr);
    auto sendmr = network.registerMr(res, sizeof(defs::GlobalAddress), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleReceivedLocks(void *recvbuf) {
    auto *lock = reinterpret_cast<defs::Lock *>(recvbuf);
    uint16_t lockId = lock->id;
    auto state = lock->state;
    std::cout << "This Lock was sent: " << lockId << ", " << state << std::endl;
    locks.insert(std::make_pair(lockId, state));
}

void Node::handleRead(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                      rdma::CompletionQueuePair *cq) {
    auto gaddr = reinterpret_cast<defs::GlobalAddress *>(recvbuf);
    auto data = read(gaddr);
    auto sendmr = network.registerMr(data, sizeof(defs::SendData), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

}

void Node::handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                       rdma::CompletionQueuePair *cq) {
    auto data = reinterpret_cast<defs::SendData *>(recvbuf);
    std::cout << "Write, SendData: data: " << data->data << ", ga-ID: " << data->ga.id
              << ", ga-size:" << data->ga.size << ", ptr: " << data->ga.ptr << ", size: "
              << data->size << std::endl;
    std::cout << id << std::endl;
    auto result = write(data);
    auto sendmr = network.registerMr(result, sizeof(defs::GlobalAddress), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleGetLocks(void *recvbuf, ibv::memoryregion::RemoteAddress remoteAddr,
                          rdma::CompletionQueuePair *cq) {
    auto nodeId = reinterpret_cast<uint16_t *>(recvbuf);
    auto lock = getLock(*nodeId);
    auto sendmr = network.registerMr(&lock, sizeof(defs::Lock), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, defs::IMMDATA::DEFAULT);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}