//
// Created by Magdalena Pröbstl on 2019-07-25.
//

#include "Connection.h"

Connection::Connection(Connection &&c) noexcept : rcqp(), socket(){
rcqp = c.rcqp;
socket = std::move(c.socket);
}

Connection::Connection(rdma::RcQueuePair *uniquePtr, l5::util::Socket s) {
    rcqp = uniquePtr;
    socket = std::move(s);
}

Connection &Connection::operator=(Connection &&other) noexcept {
    rcqp = other.rcqp;
    socket = std::move(other.socket);
    return *this;
}