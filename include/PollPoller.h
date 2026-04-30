#pragma once

#include <poll.h>
#include <vector>

#include "Poller.h"

class PollPoller : public Poller
{
public:
    explicit PollPoller(EventLoop *loop);
    ~PollPoller() override = default;

    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    using PollFdList = std::vector<struct pollfd>;
    PollFdList pollfds_;
};
