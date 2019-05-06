//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "GlobalAddress.h"


GlobalAddress::GlobalAddress(uint16_t port, char *ip): port(port), ip(ip) {
}

char* GlobalAddress::getIp(){
    return ip;
}
uint16_t GlobalAddress::getPort(){
    return port;
}