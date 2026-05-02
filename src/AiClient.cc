#include <AiClient.h>
#include <ToolRegistry.h>

#include <sstream>

namespace
{
std::string jsonEscape(const std::string &input)
{
    std::string output;
    output.reserve(input.size());
    for (char ch : input)
    {
        switch (ch)
        {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += ch;
            break;
        }
    }
    return output;
}

std::string jsonError(const std::string &message)
{
    return "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
}
}

AiClient::AiClient(const ToolRegistry *tools)
    : tools_(tools)
{
}

AiChatResponse AiClient::chat(const AiChatRequest &request) const
{
    if (!request.tool.empty())
    {
        if (tools_ == nullptr)
        {
            return AiChatResponse{false, "500 Internal Server Error", jsonError("tool registry is not configured")};
        }

        ToolResult result = tools_->execute(request.tool);
        if (!result.ok)
        {
            return AiChatResponse{false, "400 Bad Request", jsonError(result.error)};
        }

        std::ostringstream body;
        body << "{"
             << "\"ok\":true,"
             << "\"message\":\"" << jsonEscape(request.message) << "\","
             << "\"reply\":\"Tool " << jsonEscape(request.tool) << " executed for: " << jsonEscape(request.message) << "\","
             << "\"tool_used\":true,"
             << "\"tool\":\"" << jsonEscape(request.tool) << "\","
             << "\"tool_result\":" << result.json
             << "}";
        return AiChatResponse{true, "200 OK", body.str()};
    }

    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"message\":\"" << jsonEscape(request.message) << "\","
         << "\"reply\":\"Stub AI reply for: " << jsonEscape(request.message) << "\","
         << "\"tool_used\":false"
         << "}";
    return AiChatResponse{true, "200 OK", body.str()};
}
