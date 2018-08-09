//
// Created by Karven on 2017/2/26.
//

#ifndef DEXDUMP_SAFESOCKET_H
#define DEXDUMP_SAFESOCKET_H

#include <stdint.h>
#include <unistd.h>
#include "SpinLock.h"

class SafeSocket{
    int sockfd;
    int port;
    const char* addr;
    SpinLock readLock;
    SpinLock writeLock;

    int connect();
public:
    SafeSocket(int port,const char* addr);
    bool checkConnection();
    int send(const void *buf, size_t len);
    ssize_t recv(void *buf, size_t len,long timeout);
    void close(){
        ::close(sockfd);
        sockfd=-1;
    }
};

#endif //DEXDUMP_SAFESOCKET_H
