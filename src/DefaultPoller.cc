#include <stdlib.h>

#include <Poller.h>
#ifdef __linux__
#include <EPollPoller.h>
#else
#include <PollPoller.h>
#endif

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
#ifdef __linux__
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 生成poll的实例
    }
    else
    {
        return new EPollPoller(loop); // 生成epoll的实例
    }
#else
    return new PollPoller(loop);
#endif
}
