/* ************************************************************************
> File Name:     init_socket.cpp
> Author:        Luncles
> 功能：         socket初始化的一些函数
> Created Time:  Tue 23 May 2023 09:47:28 PM CST
> Description:   
 ************************************************************************/
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>

/*
 * 功能：初始化socket地址
 */
void InitSocketAddress(struct sockaddr_in &address, const char *ip, const char *port)
{
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(atoi(port));
    return;
}

/*
 * 功能：将文件描述符设置为非阻塞
 */
int SetNonblocking(int fd)
{
    int oldOption = fcntl(fd, F_GETFL);
    int newOption = oldOption | O_NONBLOCK;
    fcntl(fd, F_SETFL, newOption);
    return oldOption;
}

/*
 * 功能：将描述符注册到epoll事件集中
 */
 void addfd(int &epollfd, int &fd)
 {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;      //设置为边缘触发
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    SetNonblocking(fd);
 }

 