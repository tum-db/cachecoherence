//
// Created by Magdalena Pröbstl on 2019-07-25.
//

#include "Connection.h"

Connection::Connection(Connection &&c) noexcept : rcqp(), socket(){
rcqp = std::move(c.rcqp);
socket = std::move(c.socket);
}

Connection::Connection(std::unique_ptr<rdma::RcQueuePair> uniquePtr, l5::util::Socket s) {
    rcqp = std::move(uniquePtr);
    socket = std::move(s);
}

Connection &Connection::operator=(Connection &&other) noexcept {
    rcqp = std::move(other.rcqp);
    socket = std::move(other.socket);
    return *this;
}