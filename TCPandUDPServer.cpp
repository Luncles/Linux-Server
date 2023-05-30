/* ************************************************************************
> File Name:     TCPandUDPServer.cpp
> Author:        Luncles
> 功能：          同时处理TCP请求和UDP请求的回声服务器
> Created Time:  Tue 23 May 2023 01:38:08 AM CST
> Description:   
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include "ErrorHandling.h"
#include "init_socket.h"

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024



int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage : %s <ip> <port>\n", argv[0]);
        exit(1);
    }

    int servsock, clntsock;
    int udpsock;
    const char *ip = argv[1];
    const char *port = argv[2];
    struct sockaddr_in servAddr, clntAddr;
    socklen_t clntAddrSize;
    int readNum;

    InitSocketAddress(servAddr, ip, port);

    servsock = socket(PF_INET, SOCK_STREAM, 0);
    assert(servsock >= 0);

    int ret = bind(servsock, (struct sockaddr *)&servAddr, sizeof(servAddr));
    assert(ret != -1);

    ret = listen(servsock, 5);
    assert(ret != -1);

    /*创建UDP socket，并将其绑定到端口上*/
    InitSocketAddress(servAddr, ip, port);
    udpsock = socket(PF_INET, SOCK_DGRAM, 0);
    assert(udpsock >= 0);
    ret = bind(udpsock, (struct sockaddr *)&servAddr, sizeof(servAddr));
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    /*注册TCP socket和UDP socket上的可读事件*/
    addfd(epollfd, servsock);
    addfd(epollfd, udpsock);

    while (1)
    {
        /*等待事件发生*/
        int eventsNum = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if (eventsNum < 0)
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < eventsNum; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == servsock)         //发生连接请求事件
            {
                clntAddrSize = sizeof(clntAddr);
                clntsock = accept(servsock, (struct sockaddr *)&clntAddr, &clntAddrSize);
                addfd(epollfd, clntsock);
            }
            else if (sockfd == udpsock)     //UDP事件
            {
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                clntAddrSize = sizeof(clntAddr);
                readNum = recvfrom(udpsock, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr *)&clntAddr, &clntAddrSize);
                if (readNum > 0)
                {
                    sendto(udpsock, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr *)&clntAddr, clntAddrSize);
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                char buf[TCP_BUFFER_SIZE];
                while (1)
                {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    if (ret < 0)
                    {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        {
                            break;
                        }
                        close(sockfd);
                        break;  
                    }
                    else if (ret == 0)
                    {
                        close(sockfd);
                        break;
                    }
                    else 
                    {
                        send(sockfd, buf, ret, 0);
                    }
                }
            }
            else
            {
                printf("something else happened\n");
            }
        }
    }
    close(servsock);
    return 0;
}