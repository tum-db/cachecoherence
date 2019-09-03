//
// Created by Magdalena Pröbstl on 2019-07-25.
//

#include "Connection.h"

Connection::Connection(Connection &&c) noexcept : rcqp(), socket() {
    rcqp = std::move(c.rcqp);
    socket = std::move(c.socket);
    sendmr = std::move(c.sendmr);
    recvmr = std::move(c.recvmr);

    recvreg = c.recvreg;
    sendreg = c.sendreg;

    remoteMr = c.remoteMr;
}

Connection::Connection(std::unique_ptr<rdma::RcQueuePair> uniquePtr, l5::util::Socket s) {
    rcqp = std::move(uniquePtr);
    socket = std::move(s);
}

Connection &Connection::operator=(Connection &&other) noexcept {
    rcqp = std::move(other.rcqp);
    socket = std::move(other.socket);
    sendmr = std::move(other.sendmr);
    recvmr = std::move(other.recvmr);

    recvreg = other.recvreg;
    sendreg = other.sendreg;

    remoteMr = other.remoteMr;
    return *this;
}