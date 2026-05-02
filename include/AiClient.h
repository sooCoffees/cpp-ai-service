#pragma once

#include <string>

class ToolRegistry;

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
};

class AiClient
{
public:
    explicit AiClient(const ToolRegistry *tools = nullptr);

    AiChatResponse chat(const AiChatRequest &request) const;

private:
    const ToolRegistry *tools_;
};
