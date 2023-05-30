/* ************************************************************************
> File Name:     TimeHeap.h
> Author:        Luncles
> 功能：          时间堆实现
> Created Time:  Sun 28 May 2023 10:23:59 PM CST
> Description:   对时间堆来说，删除一个定时器节点的时间复杂度是O(1)，添加一个定时器节点的时间复杂度是O(logn),
                 执行一个定时器任务的时间复杂度是O(1)，因此，时间堆的效率很高。
 ************************************************************************/

#ifndef MIN_HEAP
#define MIN_HEAP

#include <iostream>
#include <netinet/in.h>
#include <time.h>

const int BUF_SIZE = 64;

class HeapTimeNode;

/*用户数据结构*/
struct ClientData
{
    int clntsock;
    struct sockaddr_in clntaddr;
    char dataBuf[BUF_SIZE];
    HeapTimeNode *timer;
};

/*定时器结点类*/
class HeapTimeNode
{
public:
    HeapTimeNode(int delay) { expireTimer = time(NULL) + delay; }

public:
    ClientData *userData;       //用户数据
    time_t expireTimer;         //定时器生效的绝对时间
    void (*CallBack)(ClientData *);     //回调函数
};

/*时间堆类*/
class TimeHeap
{
public:
    //构造函数一，初始化一个大小为cap的空堆。因为涉及堆，所以加了throw
    TimeHeap(int cap) throw(std::exception);
    //构造函数二，用已有数组来初始化堆
    TimeHeap(HeapTimeNode **init_array, int size, int cap) throw(std::exception);
    //析构函数
    ~TimeHeap();
    //添加目标定时器
    void AddTimerNode(HeapTimeNode *timer) throw(std::exception);
    //删除目标定时器
    void DeleteTimerNode(HeapTimeNode *timer);
    //获得堆根节点
    HeapTimeNode *TopTimer() const;
    //删除堆根节点
    void PopTimer();
    //心跳函数
    void Tick();
private:
    /*最小堆的下虑操作，确保数组中以第hole个结点为根的子树拥有最小堆性质*/
    void PercolateDown(int hole);
    /*将当前的堆数组扩容一倍*/
    void ResizeArray() throw(std::exception);
    /*判断当前堆数组是否为空*/
    bool HeapEmpty() const { return curSize == 0; }

private:
    HeapTimeNode **array;          //堆数组
    int capacity;               //堆数组的容量
    int curSize;                //堆数组当前包含元素的个数
};


#endif