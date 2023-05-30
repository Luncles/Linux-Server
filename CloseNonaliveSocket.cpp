/* ************************************************************************
> File Name:     CloseNonaliveSocket.cpp
> Author:        Luncles
> 功能：          利用升序定时器链表关闭非活动连接
> Created Time:  Fri 26 May 2023 09:28:29 PM CST
> Description:   
 ************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <assert.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include "AscendingListTimer.h"
#include "init_socket.h"

/*尽量以const代替#define */
const int MAX_EVENT_NUMBER = 1024;
const int FD_LIMIT =  65535;
const int TIMESLOT = 5;                 //定时时长
static int pipefd[2];
static SortListTimer listTimer;
static int epollfd = 0;

/*
  *功能：信号处理函数，将信号发送到管道中
  */
void SignalHandler(int sig)
{
    int oldErrno = errno;
    int message = sig;
    send(pipefd[1], (char *)&message, 1, 0);
    errno = oldErrno;
}

/*
  * 功能：添加信号到信号集合中
  */
 void AddSignal(int sig)
 {
     struct sigaction sa;
     memset(&sa, '\0', sizeof(sa));
     sa.sa_handler = SignalHandler;
     sa.sa_flags |= SA_RESTART;
     sigfillset(&sa.sa_mask);       //在信号集中设置所有信号
     assert(sigaction(sig, &sa, NULL) != -1);
 }

 /*
  * 定时器处理函数
  */
 void TimerHandler()
 {
     //定时地处理任务，其实就是调用Tick函数
     listTimer.Tick();
     //因为一次alarm调用只会引起一次SIGALRM信号，所以要重新定时，以不断触发SIGALRM信号
     alarm(TIMESLOT);
 }

 /*
  * 定时器回调函数，删除非活动连接socket上的注册事件，并关闭该socket
  */
void CallBack(ClientData *userData)
{
    //从epoll事件中删除非活动连接socket
    epoll_ctl(epollfd, EPOLL_CTL_DEL, userData->clntsock, NULL);
    assert(userData);
    close(userData->clntsock);
    printf("close socket: %d\n", userData->clntsock);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage : %s <ip> <port>\n", basename(argv[0]));
        exit(1);
    }
    const char *ip = argv[1];
    const char *port = argv[2];
    int ret = 0;
    struct sockaddr_in servAddr, clntAddr;
    InitSocketAddress(servAddr, ip, port);

    int servsock = socket(PF_INET, SOCK_STREAM, 0);
    assert(servsock >= 0);
    ret = bind(servsock, (struct sockaddr *)&servAddr, sizeof(servAddr));
    assert(ret != -1);
    ret = listen(servsock, 5);
    assert(ret != -1);
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(ret != -1);
    addfd(epollfd, servsock);
    //创建一对连接的socket
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    SetNonblocking(pipefd[1]);
    //设置读出管道的读就绪事件
    addfd(epollfd, pipefd[0]);

    /*设置信号处理函数*/
    AddSignal(SIGALRM);
    AddSignal(SIGTERM);
    bool stopServer = false;
    ClientData *users = new ClientData[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);
    while (!stopServer)
    {
        int eventNum = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((eventNum < 0) && (errno != EINTR))     //如果发生的事件小于0且不是处于中断中，那就是出错了
        {
            printf("epoll failure!\n");
            break;
        }

        /*遍历处理发生的事件*/
        for (int i = 0; i < eventNum; i++)
        {
            int sockfd = events[i].data.fd;

            /*如果是新的客户连接*/
            if (sockfd == servsock)
            {
                socklen_t clntAddrSize = sizeof(clntAddr);
                int clntsock = accept(servsock, (struct sockaddr *)&clntAddr, &clntAddrSize);
                //监听新的连接
                addfd(epollfd, clntsock);
                /*创建定时器相关：设置回调函数和定时时间，然后绑定定时器与用户数据，并加入链表*/
                users[clntsock].clntAddr = clntAddr;
                users[clntsock].clntsock = clntsock;
                UtilTimer *clntTimer = new UtilTimer();
                clntTimer->userData = &users[clntsock];
                clntTimer->callback = CallBack;
                time_t curTime = time(NULL);
                clntTimer->expire = curTime + 3 * TIMESLOT;
                users[clntsock].timer = clntTimer;
                listTimer.AddTimer(clntTimer);
            }
            /*如果有事件发生，则处理信号*/
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                char signals[MAX_EVENT_NUMBER];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)          //因为管道读是非阻塞的
                {
                    continue;
                }
                else if (ret == 0)      
                {
                    continue;
                }
                else 
                {
                    //遍历处理各个信号事件
                    for (int i = 0; i < ret; i++)
                    {
                        switch(signals[i])
                        {
                            case SIGALRM:           //定时器到
                            {
                                /*用timeout变量标记有定时任务需要处理，但不立即进行处理，因为定时任务的优先级不是很高，优先处理其他更重要的任务*/
                                timeout = true;
                                break;
                            }
                            case SIGTERM:           //终止服务器
                            {
                                stopServer = true;
                            }
                        }
                    }
                }
            }
            /*客户连接有数据接收*/
            else if (events[i].events & EPOLLIN)
            {
                memset(users[sockfd].readBuffer, '\0', BUF_SIZE);
                ret = recv(sockfd, users[sockfd].readBuffer, BUF_SIZE - 1, 0);
                printf("get %d bytes of client data : %s \n from %d\n", ret, users[sockfd].readBuffer, sockfd);
                UtilTimer *timer = users[sockfd].timer;

                if (ret < 0)
                {
                    /* 如果发生读错误，则关闭连接，并移除其定时器*/
                    if (errno != EAGAIN)            //要排除是还没读完的情况
                    {
                        CallBack(&users[sockfd]);
                        //回调函数只是移除了事件注册和关闭连接，没有移除定时器，所以需要自己移除
                        if (timer)
                        {
                            listTimer.DeleteTimer(timer);
                        }
                    }
                }
                else if (ret == 0)
                {
                    /*客户端关闭了连接，服务器端同样需要关闭连接，移除定时器*/
                    CallBack(&users[sockfd]);
                    if (timer)
                    {
                        listTimer.DeleteTimer(timer);
                    }
                }
                else
                {
                    /*因为这是实现非活动连接关闭的功能，所以当有数据传来时，表明该连接是活动的，要延缓该连接的定时时间，并调整在链表的位置*/
                    if (timer)
                    {
                        time_t curTime = time(NULL);
                        timer->expire = curTime + 3 * TIMESLOT;
                        printf("adjust timeout once\n");
                        listTimer.AdjustTimer(timer);
                    }
                }
            }
            else
            {
                ;
            }
        }
        /*处理完其他事件后，再来处理定时事件，会导致定时不准*/
        if (timeout)
        {
            TimerHandler();
            timeout = false;
        }
    }
    close(servsock);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    return 0;
}