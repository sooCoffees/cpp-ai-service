#include <errno.h>
#include <poll.h>

#include <Channel.h>
#include <Logger.h>
#include <PollPoller.h>

namespace
{
const int kNew = -1;
}

PollPoller::PollPoller(EventLoop *loop)
    : Poller(loop)
{
}

Timestamp PollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    int numEvents = ::poll(pollfds_.data(), pollfds_.size(), timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        fillActiveChannels(numEvents, activeChannels);
    }
    else if (numEvents < 0 && savedErrno != EINTR)
    {
        errno = savedErrno;
        LOG_ERROR << "PollPoller::poll() error";
    }

    return now;
}

void PollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    if (index == kNew)
    {
        struct pollfd pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size()) - 1;
        channel->set_index(idx);
        channels_[pfd.fd] = channel;
    }
    else
    {
        struct pollfd &pfd = pollfds_[index];
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        if (channel->isNoneEvent())
        {
            pfd.fd = -channel->fd() - 1;
        }
    }
}

void PollPoller::removeChannel(Channel *channel)
{
    int index = channel->index();
    channels_.erase(channel->fd());

    if (index >= 0 && index < static_cast<int>(pollfds_.size()))
    {
        int lastIndex = static_cast<int>(pollfds_.size()) - 1;
        if (index != lastIndex)
        {
            int movedFd = pollfds_[lastIndex].fd;
            if (movedFd < 0)
            {
                movedFd = -movedFd - 1;
            }
            std::swap(pollfds_[index], pollfds_[lastIndex]);
            channels_[movedFd]->set_index(index);
        }
        pollfds_.pop_back();
    }

    channel->set_index(kNew);
}

void PollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (auto pfd = pollfds_.begin(); pfd != pollfds_.end() && numEvents > 0; ++pfd)
    {
        if (pfd->revents > 0)
        {
            --numEvents;
            auto ch = channels_.find(pfd->fd);
            if (ch != channels_.end())
            {
                Channel *channel = ch->second;
                channel->set_revents(pfd->revents);
                activeChannels->push_back(channel);
            }
        }
    }
}
