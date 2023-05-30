#include "TimeHeap.h"

TimeHeap::TimeHeap(int cap) throw(std::exception) : capacity(cap), curSize(0)
{
    //注意，此时数组的每个元素都是指针，还没有指向有效的内存地址
    array = new HeapTimeNode*[capacity];       //new一个包含HeapTimeNode指针的数组
    if (!array)
    {
        throw std::exception();
    }
    for (int i = 0; i < capacity; i++)      //初始化二维数组
    {
        array[i] = NULL;
    }
}

TimeHeap::TimeHeap(HeapTimeNode **init_array, int size, int cap) throw(std::exception) : capacity(cap), curSize(size)
{
    if (cap < size)                         //容量不能比当前元素小
    {
        throw std::exception();
    }

    //创建堆数组
    array = new HeapTimeNode *[capacity];
    if (!array)
    {
        throw std::exception();
    }

    for (int i = 0; i < capacity; i++)
    {
        array[i] = NULL;
    }

    //初始化堆数组
    if (!size)
    {
        for (int i = 0; i < size; i++)
        {
            array[i] = init_array[i];
        }

        //接下来调整堆中结点的位置，只需要调整非叶子结点的位置，而在完全二叉树中，非叶子结点的个数为n/2(n为偶数)或(n-1)/2(n为奇数)
        for (int i = (curSize - 1) / 2; i >= 0; i--)
        {
            //从最后一个非叶子结点开始进行下虑操作：即当前结点和其子结点进行比较
            PercolateDown(i);
        }
    } 
}

/*
 * 析构函数：销毁时间堆
 */
TimeHeap::~TimeHeap()
{
    //因为在构造函数中，array的元素只是capacity个指针，而实际指向有效内存空间的是curSize个指针，因此需要销毁这些内存空间，然后再销毁掉指针数组
    for (int i = 0; i < curSize; i++)
    {
        delete array[i];
    }
    delete[] array;
}

/*
 * 添加目标定时器节点
 */
void TimeHeap::AddTimerNode(HeapTimeNode *timer) throw(std::exception)
{
    if (!timer)
    {
        return;
    }
    if (curSize >= capacity)        //容量已经满了，则扩容1倍
    {
        ResizeArray();
    }

    //新插入了一个元素，则堆大小要加1，hole是当前新建空穴的位置
    int hole = curSize;     
    curSize++;

    int parent = 0;
    
    //对从空穴到根节点上的路径的所有节点执行上虑操作，即本节点和父节点进行比较
    for (; hole > 0; hole = parent)
    {
        parent = (hole - 1) / 2;
        if (array[parent]->expireTimer <= timer->expireTimer)
        {
            break;
        }
        array[hole] = array[parent];
    }
    array[hole] = timer;
}

/*
 * 删除目标定时器节点
 */
void TimeHeap::DeleteTimerNode(HeapTimeNode *timer)
{
    if (!timer)
    {
        return;
    }
    //在这里只是将目标定时器的回调函数设置为空，用到了所谓的延迟销毁。将会节省真正删除该定时器造成的开销，但这样容易使堆数组膨胀
    timer->CallBack = NULL;
}

/*
 * 获得堆根节点
 */
HeapTimeNode* TimeHeap::TopTimer() const
{
    if (HeapEmpty())
    {
        return NULL;
    }
    return array[0];
}

/*
 * 删除堆根节点：先检查堆数组是否为空，再检查堆根节点是否存在，删完之后要对堆数组进行调整
 */
void TimeHeap::PopTimer()
{
    if (HeapEmpty())
    {
        return;
    }
    if (array[0])
    {
        DeleteTimerNode(array[0]);
        //将原来数组的最后一个节点放到根节点，然后进行下虑
        array[0] = array[curSize - 1];
        PercolateDown(0);
    }
}

/*
 * 心跳函数：以堆数组根节点的超时时间为触发时间
 */
void TimeHeap::Tick()
{
    HeapTimeNode *tmp = array[0];
    time_t curTime = time(NULL);        //循环处理堆中到期的定时器
    while (!HeapEmpty())
    {
        if (!tmp)
        {
            break;
        }
        
        //如果堆顶定时器还没到期，就退出循环
        if (tmp->expireTimer > curTime)
        {
            break;
        }
        //否则就执行堆顶定时器的任务
        if (array[0]->CallBack)
        {
            //回调函数
            array[0]->CallBack(array[0]->userData);
        }
        //执行完堆根节点的任务后，将根节点删除，然后再调整堆，迭代检查下个根节点
        PopTimer();
        tmp = array[0];
    }
}

/*
 * 下虑操作是迭代进行的，比如到了根节点，就会调用根节点的子节点的下虑
 * 左子结点：2*hole+1，右子节点：2*hole+2
 */
void TimeHeap::PercolateDown(int hole)
{
    //先找到以hole为头结点的那一行数组，即hole的子孙节点就在第一个结点后面
    HeapTimeNode *tmp = array[hole];
    int child = 0;
    for (;((hole * 2 + 1) <= (curSize - 1)); hole = child)
    {
        tmp = array[hole];
        //先处理左子节点
        child = 2 * hole + 1;
        //取出左右子节点之间超时值较小的结点
        if ((child < (curSize - 1)) && (array[child + 1]->expireTimer < array[child]->expireTimer))
        {
            child++;
        }
        //如果子节点的超时值小于父节点的超时值，则交换，因为都是指针，直接交换即可
        if (array[child]->expireTimer < tmp->expireTimer) 
        {
            array[hole] = array[child];     
            array[child] = tmp;
        }
        else
        {   
            // 因为是从最后一个非叶子节点开始的，所以可以保证在该节点之下的节点都具备最小堆特性，如果该节点的超时小于子节点的超时，那就小于下面所有节点的超时，可以直接退出循环
            break;
        }
    }
}

/*
 * 将当前的堆数组扩容1倍
 */
void TimeHeap::ResizeArray()
{
    HeapTimeNode **temp = new HeapTimeNode*[2 * capacity];
    for (int i = 0; i < 2 * capacity; i++)
    {
        temp[i] = NULL;
    }
    if (!temp)
    {
        throw std::exception();
    }
    for (int i = 0; i < curSize; i++)
    {
        temp[i] = array[i];
    }
    //记得删除原数组，然后重置指针
    delete[] array;
    array = temp;
}