//
// Created by Magdalena Pröbstl on 2019-06-15.
//


#include "Node.h"
#include "../util/socket/tcp.h"

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
        case 1:  //immdata = 1, if it comes from another server
        {
            handleAllocation(recvbuf, remoteMr, &cq);
            break;
        }
        case 2: //immdata = 2, if it is a reply
        {
            break;
        }
        case 3://immdata = 3, it should be freed
        {
            Free(reinterpret_cast<defs::GlobalAddress *>(recvbuf));
            break;
        }
        case 4:  //immdata = 4, write
        {
            handleWrite(recvbuf, remoteMr, &cq);
            break;
        }
        case 5:  //immdata = 5, save lock
        {
            handleReceivedLocks(recvbuf);
            break;
        }
        default: {
            return;
        }
            //    }
            //  x += 1;
    }
}

l5::util::Socket Node::connectServerSocket() {
    auto remoteAddr = rdma::Address{network.getGID(), rcqp.getQPN(), network.getLID()};
    l5::util::tcp::bind(socket, defs::port);
    l5::util::tcp::listen(socket);
    auto acced = l5::util::tcp::accept(socket);
    l5::util::tcp::write(acced, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(acced, &remoteAddr, sizeof(remoteAddr));
    rcqp.connect(remoteAddr);
    return acced;
}

void Node::connectAndReceive() {
    auto acced = connectServerSocket();
    while (true) {
        receive(&acced);
    }
}

void Node::handleAllocation(void *recvbuf, ibv::memoryregion::RemoteAddress
remoteAddr, rdma::CompletionQueuePair *cq) {
    auto size = reinterpret_cast<size_t *>(recvbuf);
    auto newgaddr = Malloc(size);
    auto sendmr = network.registerMr(newgaddr, sizeof(defs::GlobalAddress), {});
    std::cout << newgaddr->size << ", " << newgaddr->id << ", " << newgaddr->ptr
              << std::endl;
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, 0);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}

void Node::handleReceivedLocks(void *recvbuf) {
    const defs::Lock *lock = reinterpret_cast<defs::Lock *>(recvbuf);
    uint16_t lockId = lock->id;
    std::cout << "This Lock was sent: " << lockId << ", " << lock->state << std::endl;
    locks.insert(std::make_pair(lockId, lock->state));
}

void Node::handleWrite(void *recvbuf, ibv::memoryregion::RemoteAddress
remoteAddr, rdma::CompletionQueuePair *cq) {
    auto data = reinterpret_cast<defs::SendData *>(recvbuf);
    auto result = write(data);
    auto sendmr = network.registerMr(result, sizeof(defs::GlobalAddress), {});
    auto write = defs::createWriteWithImm(sendmr->getSlice(), remoteAddr, 0);
    rcqp.postWorkRequest(write);
    cq->pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
}