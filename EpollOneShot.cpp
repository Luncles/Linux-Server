#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>

#define MAX_EVENT_NUMBER 1024
#define BUF_SIZE 1024

struct fds
{
    int epollfd;
    int sockfd;    /* data */
};

/*将描述符设置为非阻塞*/
int SetNonBlocking(int fd)
{
    int oldOption = fcntl(fd, F_GETFL);
    int newOption = oldOption | O_NONBLOCK;
    fcntl(fd, F_SETFL, newOption);
    return oldOption;
}

/*将fd上的EPOLLIN和EPOLLET事件注册到epollfd指示的epoll内核事件表中，参数oneshot指定是否注册fd上的EPOLLONESHOT事件*/
void addfd(int epollfd, int fd, bool oneshot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (oneshot)
    {
        event.events |= EPOLLONESHOT;
    }
    //在epoll例程描述符epollfd中注册描述符fd，用于监视事件C
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    SetNonBlocking(fd);
}

/*重置fd上的事件，这样尽管fd上的EPOLLONESHOT事件被注册，但操作系统仍会触发fd上的EPOLLIN事件，且只触发一次*/
void ResetOneShot(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/*工作线程*/
void *EpollWorkerMain(void *arg)
{
    int sockfd = ((fds *)arg)->sockfd;
    int epollfd = ((fds *)arg)->epollfd;
    printf("start new thread to receive data on fd:%d\n", sockfd);
    char buf[BUF_SIZE];
    memset(buf, '\0', BUF_SIZE);

    /*循环读取sockfd上的数据，因为是非阻塞的，所以当遇到EAGAIN错误时表示还不能读*/
    while (1)
    {
        int ret = recv(sockfd, buf, BUF_SIZE - 1, 0);
        
        /*收到0表示断开连接*/
        if (ret == 0)
        {
            close(sockfd);
            printf("closed the connection\n");
            break;
        }
        else if (ret < 0)
        {
            if (errno == EAGAIN)    //暂时还不能读
            {
                ResetOneShot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }
        else
        {
            printf("get content:%s\n", buf);
            /*休眠5秒，模拟数据处理过程*/
            sleep(5);
        }
    }
    printf("end thread receiving data on fd:%d\n", sockfd);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <ip><port>\n", basename(argv[0]));
        exit(1);
    }
    int servSock, clntSock;
    struct sockaddr_in servAddr, clntAddr;
    socklen_t clntAddrSize;

    servSock = socket(PF_INET, SOCK_STREAM, 0);
    assert(servSock >= 0);

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(argv[1]);
    servAddr.sin_port = htons(atoi(argv[2]));

    int ret = bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr));
    assert(ret != -1);
    ret = listen(servSock, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    /*监听socket servSock上是不能注册EPOLLONESHOT事件的，否则应用程序只能处理一个客户连接，因为后续的客户连接请求将不再触发servSock上的EPOLLIN事件*/
    addfd(epollfd, servSock, false);
    while (1)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0)
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < ret; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == servSock)
            {
                clntAddrSize = sizeof(clntAddr);
                clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrSize);

                /*对每个非监听连接的文件描述符都注册EPOLLONESHOT事件*/
                addfd(epollfd, clntSock, true);
            }
            else if (events[i].events & EPOLLIN)    /*如果是读取数据事件*/
            {
                /*新开一个工作线程为sockfd服务*/
                pthread_t pid;
                fds newWorkerFds;
                newWorkerFds.epollfd = epollfd;
                newWorkerFds.sockfd = sockfd;

                pthread_create(&pid, NULL, EpollWorkerMain, (void *)&newWorkerFds);
            }
            else
            {
                printf("something else happened\n");
            }
        }
    }
    close(servSock);
    return 0;
}

