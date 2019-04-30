//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#include <cstdint>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "GlobalAddress.h"


GlobalAddress::GlobalAddress(uint16_t port, char *ip): port(port), ip(ip) {

}

sockaddr_in GlobalAddress::getSockAddr(){
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    return addr;
}
