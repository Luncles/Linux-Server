/* ************************************************************************
> File Name:     AscendingListTimer.h
> Author:        Luncles
> 功能：          创建升序定时器链表
> Created Time:  Thu 25 May 2023 10:28:17 PM CST
> Description:   
 ************************************************************************/

#ifndef LST_TIMER
#define LST_TIMER
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUF_SIZE 64

/*前向声明*/
class UtilTimer;

/*用户数据结构：客户端socket地址、socket文件描述符、读缓存和定时器*/
struct ClientData
{
    sockaddr_in clntAddr;
    int clntsock;
    char readBuffer[BUF_SIZE];
    UtilTimer *timer;
};

/*定时器类*/
class UtilTimer
{
    friend class SortListTimer;     //声明友元类，以便访问私有数据
public:
    /*默认构造函数*/
    UtilTimer() : expire(0), userData(nullptr), prev(nullptr), next(nullptr) { }
    /*任务回调函数*/
    void (*callback)(ClientData *); 
    

public:
    time_t expire;          //任务的超时时间，使用的是绝对时间
    ClientData *userData;   //回调函数处理的客户数据，由定时器的调用者传递给回调函数
    UtilTimer *prev;        //前一个定时器
    UtilTimer *next;        //后一个定时器
};

/*
 * 定时器链表：是一个升序的双向链表，且带有头结点和尾结点
 */
class SortListTimer
{
public:
    /*默认构造函数*/
    SortListTimer() : head(nullptr), tail(nullptr) { }
    /*析构函数*/
    ~SortListTimer();
    /*添加定时器*/
    void AddTimer(UtilTimer *timer);
    /*调整定时器位置*/
    void AdjustTimer(UtilTimer *timer);
    /*删除目标定时器*/
    void DeleteTimer(UtilTimer *timer);
    /*心跳函数*/
    void Tick();

private:
    void AddTimer(UtilTimer *timer, UtilTimer *lstHead);

private:
    UtilTimer *head;
    UtilTimer *tail;
};

/*
 * 链表被销毁时，删除其中所有的定时器
 */
SortListTimer::~SortListTimer()
{
    UtilTimer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

/*
 * 将目标定时器添加到链表中
 */
void SortListTimer::AddTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!head)
    {
        head = tail = timer;
        return;
    }
    /*如果目标定时器的超时时间小于表头，则把目标定时器插入表头*/
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    /*调用重载函数将目标定时器插入链表中*/
    AddTimer(timer, head);
}

/*
 * 当某个定时任务发生变化时，其定时器在链表中的位置也会发生改变，这里只考虑定时时间增加的情况，即目标定时器向后调整
 * 考虑3种情况：1、当目标定时器本来就在链尾，2、变化后还是小于原来的下一个结点，则不用调整
 * 3、如果目标定时器原本是表头，则还需要额外操作
 * 4、剩下的情况，需要把目标定时器拿出来，还要串联目标定时器原来的前后结点，再插入到合适位置
 */
void SortListTimer::AdjustTimer(UtilTimer *timer)
{
    UtilTimer *tmp = timer->next;
    if (!timer)
    {
        return;
    }
    //情况1，2
    if (timer == tail || timer->expire < tmp->expire)
    {
        return;
    }
    //情况3
    if (timer == head)
    {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        AddTimer(timer, head);
    }
    //情况4
    else
    {
        timer->prev->next = tmp;
        tmp->prev = timer->prev;
        AddTimer(timer, tmp);
    }

}

/*
 * 从链表中删除目标定时器
 * 考虑4种情况：1、链表中没有结点：直接返回；2、链表中只有一个结点：删完之后要置nullptr；3、删除的是头/尾结点：删完之后更新头/尾结点；
 * 4、其他情况:删完要串起前后结点
 */
void SortListTimer::DeleteTimer(UtilTimer *timer)
{
    //情况1
    if (!timer || !head)
    {
        return;
    }
    //情况2
    if (head == timer && tail == timer)
    {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    //情况3
    if (timer == head)
    {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    //情况4
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

/*
 * SIGALRM信号每次被触发就在其信号处理函数（如果使用统一事件源，则是主函数）中执行一次Tick函数，以处理链表上到期的任务
 */
 void SortListTimer::Tick()
 {
     if (!head)
     {
        return;
     }
    printf("timer tick\n");
    time_t curTime = time(NULL);
    UtilTimer *tmp = head;
     
     /*从头结点开始依次处理定时事件，直到遇到未到期的定时器为止*/
     while (tmp)
     {
         /*因为定时器使用的是绝对时间，所以可以直接进行比较*/
         if (curTime < tmp->expire)
         {
             break;
         }
         /*调用定时器的回调函数，执行定时任务*/
         tmp->callback(tmp->userData);
         /*执行完定时任务后，将其从链表中删除，并重置头结点*/
         head = tmp->next;
         if (head)
         {
            head->prev = nullptr;
         }
        delete tmp;
        tmp = head;
     }
 }

/*
 * 重载的结点移动函数，目标结点不是头尾结点
 */
 void SortListTimer::AddTimer(UtilTimer *timer, UtilTimer *lstHead)
 {
    while (timer->expire >= lstHead->expire && lstHead)
    {
        lstHead = lstHead->next;
    }
    if (lstHead)
    {
        timer->next = lstHead;
        timer->prev = lstHead->prev;
        lstHead->prev = timer;
    }
    else        //插入链表尾部
    {
        timer->prev = tail;
        tail->next = timer;
        timer->next = nullptr;
        tail = timer;
    } 
 }

#endif