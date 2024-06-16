#include "timer.h"

/**
 * 下滤：将当前节点与其左、右子节点相比，如果当前节点的值比其中一个（或两个）子节点的值大，就把当前节点与两个子节点中较小的那个交换，
 *      继续前面的比较，直到当前节点的值比两个子节点的值都小为止。此时，便符合最小堆的定义。
 */
bool Timer::siftdown_(size_t index, size_t n)
{ // 从index到n之间进行下虑
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while (j < n) {
        if (j + 1 < n && heap_[j + 1] < heap_[j])
            j++;
        if (heap_[i] < heap_[j])
            break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

/**
 * 上滤：将当前节点与其父节点相比，如果当前节点的值比较小，就把当前节点与父节点交换，
 *      继续前面的比较，直到当前节点的值比父节点的值大为止。此时，便符合最小堆的定义。
 */
void Timer::siftup_(size_t i)
{
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while (j >= 0) {
        if (heap_[j] < heap_[i]) {
            break;
        }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}
void Timer::SwapNode_(size_t i, size_t j)
{
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

/**
 * 交换要删除节点和最后一个节点，然后进行下滤和上滤操作，最后删除数组尾部元素
 */
void Timer::del_(size_t index)
{
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if (i < n) {
        SwapNode_(i, n);
        if (!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void Timer::adjust(int id, int timeout)
{
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    ;
    siftdown_(ref_[id], heap_.size());
}

void Timer::add(int id, int timeOut, const TimeoutCallBack& cb)
{
    assert(id >= 0);
    size_t i;
    if (ref_.count(id) == 0) { /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({ id, Clock::now() + MS(timeOut), cb });
        siftup_(i);
    } else { /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeOut);
        heap_[i].cb = cb;
        if (!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
}

void Timer::clear()
{
    ref_.clear();
    heap_.clear();
}

void Timer::tick()
{
    /* 清除超时结点 */
    if (heap_.empty()) {
        return;
    }
    while (!heap_.empty()) {
        TimerNode node = heap_.front();
        // 使用std::chrono::duration_cast进行时间转换
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

int Timer::GetNextTick()
{
    tick();
    size_t res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0) {
            res = 0;
        }
    }
    return res;
}

void Timer::pop()
{
    assert(!heap_.empty());
    del_(0);
}