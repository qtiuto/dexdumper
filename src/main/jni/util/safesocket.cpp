//
// Created by Karven on 2017/2/26.
//

#include "safesocket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <asm/fcntl.h>

int set_socket_nonblock(int sockfd)
{
    int block_flag = fcntl(sockfd, F_GETFL, 0);
    if(block_flag < 0){
        printf("get socket fd flag error:%s\n", strerror(errno));
        return -1;
    }
    else{
        if(fcntl(sockfd, F_SETFL, block_flag | O_NONBLOCK) < 0){
            printf("set socket fd non block error:%s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}
int set_socket_block(int sockfd)
{
    int block_flag = fcntl(sockfd, F_GETFL, 0);
    if(block_flag < 0){
        printf("get socket fd flag error:%s\n", strerror(errno));
        return -1;
    }
    else{
        if(fcntl(sockfd, F_SETFL, block_flag&(~O_NONBLOCK)) < 0){
            printf("set socket fd block error:%s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

 ssize_t socket_nonblock_recv(int fd, void *buffer, size_t length, unsigned long timeout)
{
    size_t bytes_left;
    long long read_bytes;
    unsigned char * ptr;
    ptr = (unsigned char *) buffer;
    bytes_left = length;
    fd_set readfds;
    int ret = 0;
    struct timeval tv;
    while(bytes_left > 0){
        read_bytes = recv(fd, ptr, bytes_left, 0);
        if(read_bytes < 0){
            if(errno == EINTR)      //由于信号中断
                read_bytes = 0;
            else if(errno == EAGAIN){    //EAGAIN 没有可读写数据,缓冲区无数据
                if(length > bytes_left)    //说明上一循环把缓冲区数据读完，继续读返回-1，应返回已读取的长度
                    return (length - bytes_left);
                else{    //length == bytes_left,说明第一次调用该函数就无数据可读，可能是对端无数据发来，可能是对端网线断了
                    FD_ZERO(&readfds);
                    FD_SET(fd, &readfds);
                    tv.tv_sec = timeout/1000000;
                    tv.tv_usec = timeout%1000000;
                    ret = select(fd+1, &readfds, NULL, NULL, &tv); //阻塞,err:0 timeout err:-1 错误见errno
                    if(ret == 0){    //超时，判定为网线断开
                        printf("select error:%s\n", strerror(errno));
                        return -2;
                    }
                    else if(ret < 0 && errno != EINTR) {
                        printf("select error:%s\n", strerror(errno));
                        return -2;
                    }
                    //未超时，有数据到来，继续读
                    continue;
                }
            }
            else {      //其他错误
                printf("read socket buf error:%s\n", strerror(errno));
                return -3;
            }
        }
        else if(read_bytes == 0){    //缓冲区数据读完，对端fd 关闭或对端没有发数据了，超时10s后判定为连接已断
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            tv.tv_sec = timeout/1000000;
            tv.tv_usec = timeout%1000000;
            ret = select(fd+1, NULL, NULL, &readfds, &tv); //阻塞,err:0 timeout err:-1 错误见errno
            if(ret == 0){   //超时，对端fd关闭，或对端没有再发数据
                printf("select error:%s\n", strerror(errno));
                return -1;
            }
            else if(ret < 0 && errno != EINTR){
                printf("select error:%s\n", strerror(errno));
                return -1;
            }
            //未超时，有数据到来，继续读
            continue;
        }
        bytes_left -= read_bytes;
        ptr += read_bytes;
    }
    return (length - bytes_left);
}

ssize_t socket_block_recv(int fd, void *buffer, size_t length)
{
    size_t bytes_left;
    long long read_bytes;
    unsigned char * ptr;
    ptr = (unsigned char *) buffer;
    bytes_left = length;
    struct timeval tv;
    while(bytes_left > 0){
        read_bytes = recv(fd, ptr, bytes_left, 0);
        if(read_bytes == 0){
            break;
        }
        if (read_bytes<0){
            return -1;
        }
        bytes_left -= read_bytes;
        ptr += read_bytes;
    }
    return (length - bytes_left);
}

int socket_block_send(int fd, const void* buffer,  size_t length)
{
    if(length == 0 || buffer == NULL){
        printf("buffer point is NULL or length is zero\n");
        return 0;
    }
    size_t bytes_left;  //无符号
    ssize_t written_bytes;  //有符号
    const unsigned char* ptr;
    ptr = (const unsigned char *) buffer;
    bytes_left = length;
    struct timeval tv;
    int ret = 0;
    while(bytes_left > 0){
        written_bytes = send(fd, ptr, bytes_left, MSG_NOSIGNAL|MSG_CONFIRM);
        if(written_bytes < 0){
            printf("write socket error %d:%s\n", errno, strerror(errno));
            return (int)written_bytes;//

        }
        bytes_left -= written_bytes;
        ptr += written_bytes;
    }
    return 0;
}

SafeSocket::SafeSocket(int port, const char *addr):port(port),addr(addr),sockfd(-1) {}

int SafeSocket::connect() {
    if(sockfd==-1){
        sockfd=socket(AF_INET,SOCK_STREAM,0);
        if(sockfd==-1) {
            return -1;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        //设置TCP/IP连接
        addr.sin_family=AF_INET;
        //设置端口号
        addr.sin_port = htons(port);
        //设置允许连接地址
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        int numx=::connect(sockfd,(sockaddr*)&addr,sizeof(addr));
        if(numx==-1) {
            return -1;
        }
    }
    return 0;
}
bool SafeSocket::checkConnection() {
    return connect() != -1;
}


int SafeSocket::send(const void *buf, size_t len) {
    writeLock.lock();
    if(!checkConnection()) return -1;
    int ret= socket_block_send(sockfd,buf,len);
    if(ret<0) {
        int err=errno;
        if(err==EPIPE||err==EOPNOTSUPP||err==ENOTSOCK||err==ENOTCONN){
            SafeSocket::close();
        }
    }
    writeLock.unlock();
    return ret;
}
ssize_t SafeSocket::recv(void *buf, size_t len, long timeout) {
    readLock.lock();
    ssize_t ret;
    if(timeout<=0){
        ret=socket_block_recv(sockfd,buf,len);
    } else{
        set_socket_nonblock(sockfd);
        ret=socket_nonblock_recv(sockfd, buf, len, (unsigned long) timeout);
        set_socket_block(sockfd);
    }
    if(ret<0) {
        int err=errno;
        if(err==EBADF||err==ECONNREFUSED||err==ENOTSOCK||err==ENOTCONN){
            SafeSocket::close();
        }
    }
    readLock.unlock();
    return ret;
}

