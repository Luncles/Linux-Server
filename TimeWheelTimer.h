/* ************************************************************************
> File Name:     TimeWheelTimer.h
> Author:        Luncles
> 功能：          简易时间轮实现代码
> Created Time:  Sat 27 May 2023 10:10:47 PM CST
> Description:   
 ************************************************************************/

#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

const int BUF_SIZE = 64;

class TimeWheelTimer;

/*用户数据结构*/
struct ClientData
{
    int clntsock;
    struct sockaddr_in clntAddr;
    char buf[BUF_SIZE];
    TimeWheelTimer *clntTimer;
};

/*定时器类*/
class TimeWheelTimer
{
public:
    TimeWheelTimer(int rotaNum, int ts) : prev(nullptr), next(nullptr), rotationNum(rotaNum), timeSlot(ts) { }
    ~TimeWheelTimer();

public:
    TimeWheelTimer *prev;           //指向下一个定时器
    TimeWheelTimer *next;           //指向前一个定时器
    int rotationNum;                //记录定时器在时间轮转多少圈后生效
    int timeSlot;                   //记录定时器属于时间轮上哪个槽
    ClientData *userData;           //保存用户数据
    void (*CallBack)(ClientData *); //定时器回调函数
};

/*时间轮类*/
class TimeWheel
{
public:
    TimeWheel() : curSlot(0)
    {
        for (int i = 0; i < numSlot; i++)
        {
            slots[i] = nullptr;             //初始化每个槽的头结点
        }
    }
    ~TimeWheel(); 
    //根据定时值创建定时器，并插入槽中
    TimeWheelTimer *AddTimer(int timeout);
    //删除定时器
    void DeleteTimer(TimeWheelTimer *timer);
    //心跳函数
    void Tick();

private:
    int curSlot;                        //时间轮的当前槽
    static const int numSlot = 60;      //时间轮上槽的数目
    static const int rotateTime = 1;    //每隔1秒时间轮转动一次
    TimeWheelTimer *slots[numSlot];     //时间轮的槽，每个槽指向一个定时器链表，链表无序
};

TimeWheel::~TimeWheel()
{
    for (int i = 0; i < numSlot; i++)
    {
        TimeWheelTimer *tmp = slots[i];
        while (tmp)
        {
            slots[i] = tmp->next;
            delete tmp;
            tmp = slots[i];
        }
    }
}

/*
 * 根据定时值timeout创建一个定时器，并将其插入到合适的槽中
 */
TimeWheelTimer * TimeWheel::AddTimer(int timeout)
{
    if (timeout < 0)
    {
        return nullptr;
    }

    int ticks = 0;
    /*下面根据timeout算出该定时器在时间轮转动多少个槽后会被触发，并将该滴答数存储在变量ticks中，如果待插入定时器的
    超时值小于时间轮的槽间隔rotateTime，那么将ticks向上取整为1，如果大于槽间隔，就向下取整为timeout/rotateTime*/
    if (timeout <= rotateTime)
    {
        ticks = 1;
    }
    else
    {
        ticks = timeout / rotateTime;
    }

    /*根据超时时间计算时间轮转动多少圈后会被触发*/
    int rotation = ticks / numSlot;
    /*计算待插入的定时器应被插在哪个槽里*/
    int insertSlot = (curSlot + ticks % numSlot) % numSlot;
    /*创建新的定时器，在时间轮转动rotation圈后被触发，且位于第insertSlot个槽上*/
    TimeWheelTimer *timer  = new TimeWheelTimer(rotation, insertSlot);

    /*如果第insertSlot个槽上没有任何定时器，就将新建的定时器插入其中，并将该定时器设置为该槽的头结点*/
    if (!slots[insertSlot])
    {
        printf("add timer, rotation is %d, insertslot is %d, curSlot is %d\n", rotation, insertSlot, curSlot);
        slots[insertSlot] = timer;
    }
    /*否则，将定时器插入到第insertSlot个槽中*/
    else
    {
        /*头插法*/
        timer->next = slots[insertSlot];
        slots[insertSlot]->prev = timer;
        slots[insertSlot] = timer;
    }
    return timer;
}

/*
 * 删除目标定时器
 */
void TimeWheel::DeleteTimer(TimeWheelTimer *timer)
{
    if (!timer)
    {
        return;
    }
    int targetSlot = timer->timeSlot;
    /*slots[targetSlot]就是目标定时器所在的槽的头结点。如果目标定时器就是该头结点，则需要重置头结点*/
    if (timer == slots[targetSlot])
    {
        slots[targetSlot] = slots[targetSlot]->next;
        if (slots[targetSlot])
        {
            slots[targetSlot]->prev = nullptr;
        }
        delete timer;
    }
    else
    {
        timer->prev->next = timer->next;
        if (timer->next)
        {
            timer->next->prev = timer->prev;
        }
        delete timer;
    }
}

/*
 * 当一个心跳时间rotateTime到后，调用该函数，执行当前槽上到期的任务，然后时间轮向前滚动一个槽的间隔
 */
void TimeWheel::Tick()
{
    TimeWheelTimer *tmp = slots[curSlot];   //取得时间轮上当前槽的头结点
    printf("current slot is %d\n", curSlot);
    while (tmp)
    {
        printf("tick the timer once\n");
        /*如果定时器的rotationNum值大于0，则当前结点的任务在这一圈还没到期，不用触发*/
        if (tmp->rotationNum > 0)
        {
            tmp->rotationNum--;
            tmp = tmp->next;
        }
        /*如果rotationNum=0，说明该结点的任务在这一圈就会被触发，于是执行定时任务，然后删除该定时器*/
        else
        {
            tmp->CallBack(tmp->userData);
            if (tmp == slots[curSlot])
            {
                printf("delete header in curSlot\n");
                slots[curSlot] = slots[curSlot]->next;
                delete tmp;
                if (slots[curSlot])
                {
                    slots[curSlot]->prev = nullptr;
                }
                tmp = slots[curSlot];
            }
            else
            {
                tmp->prev->next = tmp->next;
                if (tmp->next)
                {
                    tmp->next->prev = tmp->prev;
                }
                TimeWheelTimer *tmp2 = tmp->next;
                delete tmp;
                tmp = tmp2;
            }
        }
    }
    /*执行完当前周期所有到期的任务之后，就转动时间轮到下一个槽*/
    curSlot = (curSlot + 1) % numSlot;
}

#endif