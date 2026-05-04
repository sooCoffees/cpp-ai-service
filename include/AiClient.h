#pragma once

#include <string>

#include "LFU.h"

class ToolRegistry;

struct AiClientConfig
{
    std::string provider;
    std::string model;
    std::string baseUrl;
    std::string apiKey;
    bool apiKeyConfigured;
    int cacheCapacity;
    int requestTimeoutSeconds;

    static AiClientConfig fromEnvironment();
};

struct AiChatRequest
{
    std::string message;
    std::string tool;
};

struct AiChatResponse
{
    bool ok;
    std::string status;
    std::string body;
    bool cacheHit;
    bool toolUsed;
    std::string toolName;
    long upstreamLatencyMs;
};

class AiClient
{
public:
    explicit AiClient(const ToolRegistry *tools = nullptr,
                      const AiClientConfig &config = AiClientConfig::fromEnvironment());

    AiChatResponse chat(const AiChatRequest &request);
    std::string configJson() const;

private:
    std::string cacheKey(const AiChatRequest &request) const;
    std::string makeStubReply(const std::string &message) const;
    AiChatResponse callOpenAiCompatible(const AiChatRequest &request) const;
    bool shouldUseNetworkProvider() const;
    std::string buildChatBody(const AiChatRequest &request,
                              const std::string &reply,
                              bool cacheHit) const;

    const ToolRegistry *tools_;
    AiClientConfig config_;
    KamaCache::KHashLfuCache<std::string, std::string> responseCache_;
};
