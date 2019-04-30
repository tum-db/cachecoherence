//
// Created by Magdalena Pröbstl on 2019-04-30.
//

#ifndef MEDMM_GLOBALADDRESS_H
#define MEDMM_GLOBALADDRESS_H


class GlobalAddress {
private:
    uint16_t port;
    char *ip;
public:
    GlobalAddress(uint16_t port,char *ip);
    sockaddr_in getSockAddr();
};


#endif //MEDMM_GLOBALADDRESS_H
