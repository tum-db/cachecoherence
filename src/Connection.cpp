//
// Created by Magdalena Pröbstl on 2019-07-25.
//

#include <thread>
#include "Connection.h"
#include "../util/defs.h"
#include "../util/socket/tcp.h"

Connection::Connection(Connection &&c) noexcept {

    rcqp = std::move(c.rcqp);
    sendmr = std::move(c.sendmr);
    recvmr = std::move(c.recvmr);

    recvreg = c.recvreg;
    sendreg = c.sendreg;

    remoteMr = c.remoteMr;

    c.recvreg = nullptr;
    c.sendreg = nullptr;
}

Connection::Connection(rdma::Network &network) {
    rcqp = std::make_unique<rdma::RcQueuePair>(
            rdma::RcQueuePair(network, network.getSharedCompletionQueue()));

    recvreg = static_cast<char *>(malloc(defs::BIGBADBUFFER_SIZE*2));
    sendreg = static_cast<char *>(malloc(defs::MAX_BLOCK_SIZE));

    sendmr = network.registerMr(sendreg, defs::MAX_BLOCK_SIZE, {});
    recvmr = network.registerMr(recvreg, defs::BIGBADBUFFER_SIZE*2,
                                  {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvreg),
                                                recvmr->getRkey()};
}

Connection &Connection::operator=(Connection &&other) noexcept {
    rcqp = std::move(other.rcqp);
    sendmr = std::move(other.sendmr);
    recvmr = std::move(other.recvmr);

    recvreg = other.recvreg;
    sendreg = other.sendreg;

    remoteMr = other.remoteMr;

    other.recvreg = nullptr;
    other.sendreg = nullptr;

    return *this;
}

void Connection::connect(rdma::Network &network, l5::util::Socket &socket){
    auto remoteAddr = rdma::Address{network.getGID(), rcqp->getQPN(), network.getLID()};
    l5::util::tcp::write(socket, &remoteAddr, sizeof(remoteAddr));
    l5::util::tcp::read(socket, &remoteAddr, sizeof(remoteAddr));

    l5::util::tcp::write(socket, &remoteMr, sizeof(remoteMr));
    l5::util::tcp::read(socket, &remoteMr, sizeof(remoteMr));
    rcqp->connect(remoteAddr);
    socket.close();
}

void Connection::close(){
    rcqp->setToResetState();
    remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvreg),
                                                recvmr->getRkey()};
}
/*
Connection::~Connection() {
    std::cout << "DESTRUCTION" << std::endl;
    free(recvreg);
    free(sendreg);

}
*/