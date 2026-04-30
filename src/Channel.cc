#ifdef __linux__
#include <sys/epoll.h>
#else
#include <poll.h>
#endif

#include <Channel.h>
#include <EventLoop.h>
#include <Logger.h>

const int Channel::kNoneEvent = 0; //空事件
#ifdef __linux__
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; //读事件
const int Channel::kWriteEvent = EPOLLOUT; //写事件
#else
const int Channel::kReadEvent = POLLIN | POLLPRI; //读事件
const int Channel::kWriteEvent = POLLOUT; //写事件
#endif

// EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{
}

Channel::~Channel()
{
}

// channel的tie方法什么时候调用过?  TcpConnection => channel
/**
 * TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection
 * 对象的成员方法，因此可以说明一点就是：Channel的结束一定晚于TcpConnection对象！
 * 此处用tie去解决TcpConnection和Channel的生命周期时长问题，从而保证了Channel对象能够在
 * TcpConnection销毁前销毁。
 **/
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}
//update 和remove => EpollPoller 更新channel在poller中的状态
/**
 * 当改变channel所表示的fd的events事件后，update负责再poller里面更改fd相应的事件epoll_ctl
 **/
void Channel::update()
{
    // 通过channel所属的eventloop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中把当前的channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
        // 如果提升失败了 就不做任何处理 说明Channel的TcpConnection对象已经不存在了
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO<<"channel handleEvent revents:"<<revents_;
    // 关闭
    if ((revents_ & POLLHUP) && !(revents_ & kReadEvent)) // 当TcpConnection对应Channel 通过shutdown 关闭写端触发HUP
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    // 错误
    if (revents_ & POLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    // 读
    if (revents_ & kReadEvent)
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    // 写
    if (revents_ & kWriteEvent)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
