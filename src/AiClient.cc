#include <AiClient.h>
#include <ToolRegistry.h>

#include <cstdlib>
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

std::string envOrDefault(const char *name, const std::string &fallback)
{
    const char *value = std::getenv(name);
    if (value == nullptr || *value == '\0')
    {
        return fallback;
    }
    return value;
}

int positiveEnvOrDefault(const char *name, int fallback)
{
    const char *value = std::getenv(name);
    if (value == nullptr || *value == '\0')
    {
        return fallback;
    }

    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || parsed <= 0)
    {
        return fallback;
    }

    return static_cast<int>(parsed);
}
}

AiClientConfig AiClientConfig::fromEnvironment()
{
    AiClientConfig config;
    config.provider = envOrDefault("CPP_AI_PROVIDER", "stub");
    config.model = envOrDefault("CPP_AI_MODEL", "stub-local");
    const char *apiKey = std::getenv("OPENAI_API_KEY");
    config.apiKeyConfigured = apiKey != nullptr && *apiKey != '\0';
    config.cacheCapacity = positiveEnvOrDefault("CPP_AI_CACHE_CAPACITY", 64);
    return config;
}

AiClient::AiClient(const ToolRegistry *tools, const AiClientConfig &config)
    : tools_(tools)
    , config_(config)
    , responseCache_(config.cacheCapacity, 4)
{
}

AiChatResponse AiClient::chat(const AiChatRequest &request)
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
             << "\"provider\":\"" << jsonEscape(config_.provider) << "\","
             << "\"model\":\"" << jsonEscape(config_.model) << "\","
             << "\"cache_hit\":false,"
             << "\"tool_used\":true,"
             << "\"tool\":\"" << jsonEscape(request.tool) << "\","
             << "\"tool_result\":" << result.json
             << "}";
        return AiChatResponse{true, "200 OK", body.str()};
    }

    const std::string key = cacheKey(request);
    std::string cachedReply;
    if (responseCache_.get(key, cachedReply))
    {
        return AiChatResponse{true, "200 OK", buildChatBody(request, cachedReply, true)};
    }

    const std::string reply = makeStubReply(request.message);
    responseCache_.put(key, reply);
    return AiChatResponse{true, "200 OK", buildChatBody(request, reply, false)};
}

std::string AiClient::configJson() const
{
    std::ostringstream body;
    body << "{"
         << "\"provider\":\"" << jsonEscape(config_.provider) << "\","
         << "\"model\":\"" << jsonEscape(config_.model) << "\","
         << "\"api_key_configured\":" << (config_.apiKeyConfigured ? "true" : "false") << ","
         << "\"cache_capacity\":" << config_.cacheCapacity
         << "}";
    return body.str();
}

std::string AiClient::cacheKey(const AiChatRequest &request) const
{
    return config_.provider + "\n" + config_.model + "\n" + request.message;
}

std::string AiClient::makeStubReply(const std::string &message) const
{
    return "Stub AI reply for: " + message;
}

std::string AiClient::buildChatBody(const AiChatRequest &request,
                                    const std::string &reply,
                                    bool cacheHit) const
{
    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"message\":\"" << jsonEscape(request.message) << "\","
         << "\"reply\":\"" << jsonEscape(reply) << "\","
         << "\"provider\":\"" << jsonEscape(config_.provider) << "\","
         << "\"model\":\"" << jsonEscape(config_.model) << "\","
         << "\"cache_hit\":" << (cacheHit ? "true" : "false") << ","
         << "\"tool_used\":false"
         << "}";
    return body.str();
}
