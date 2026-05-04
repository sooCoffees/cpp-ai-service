#include <AiClient.h>
#include <ToolRegistry.h>

#include <curl/curl.h>

#include <chrono>
#include <cstdlib>
#include <sstream>

namespace
{
const char *kOpenAiProvider = "openai";
const char *kOpenAiCompatibleProvider = "openai_compatible";
const char *kDeepSeekProvider = "deepseek";
const char *kOpenRouterProvider = "openrouter";
const char *kGroqProvider = "groq";
const char *kOllamaProvider = "ollama";

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

bool extractJsonStringField(const std::string &json, const std::string &field, std::string *value)
{
    const std::string key = "\"" + field + "\"";
    size_t keyPos = json.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t colon = json.find(':', keyPos + key.size());
    if (colon == std::string::npos)
    {
        return false;
    }

    size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos)
    {
        return false;
    }

    std::string parsed;
    bool escaped = false;
    for (size_t i = quote + 1; i < json.size(); ++i)
    {
        char ch = json[i];
        if (escaped)
        {
            switch (ch)
            {
            case '"':
            case '\\':
            case '/':
                parsed += ch;
                break;
            case 'n':
                parsed += '\n';
                break;
            case 'r':
                parsed += '\r';
                break;
            case 't':
                parsed += '\t';
                break;
            default:
                parsed += ch;
                break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\')
        {
            escaped = true;
            continue;
        }

        if (ch == '"')
        {
            *value = parsed;
            return true;
        }

        parsed += ch;
    }

    return false;
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

std::string firstEnvValue(const char *first,
                          const char *second,
                          const char *third,
                          const char *fallback)
{
    const char *names[] = {first, second, third};
    for (size_t i = 0; i < 3; ++i)
    {
        if (names[i] == nullptr)
        {
            continue;
        }

        const char *value = std::getenv(names[i]);
        if (value != nullptr && *value != '\0')
        {
            return value;
        }
    }
    return fallback;
}

std::string defaultBaseUrl(const std::string &provider)
{
    if (provider == kDeepSeekProvider)
    {
        return "https://api.deepseek.com/v1";
    }
    if (provider == kOpenRouterProvider)
    {
        return "https://openrouter.ai/api/v1";
    }
    if (provider == kGroqProvider)
    {
        return "https://api.groq.com/openai/v1";
    }
    if (provider == kOllamaProvider)
    {
        return "http://127.0.0.1:11434/v1";
    }
    return "https://api.openai.com/v1";
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

size_t writeCurlResponse(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    std::string *response = static_cast<std::string *>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

void ensureCurlGlobalInit()
{
    struct CurlGlobal
    {
        CurlGlobal()
        {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }

        ~CurlGlobal()
        {
            curl_global_cleanup();
        }
    };

    static CurlGlobal global;
    (void)global;
}

std::string stripTrailingSlash(std::string value)
{
    while (!value.empty() && value[value.size() - 1] == '/')
    {
        value.erase(value.size() - 1);
    }
    return value;
}
}

AiClientConfig AiClientConfig::fromEnvironment()
{
    AiClientConfig config;
    config.provider = envOrDefault("CPP_AI_PROVIDER", "stub");
    config.model = envOrDefault("CPP_AI_MODEL", "stub-local");
    config.baseUrl = envOrDefault("CPP_AI_BASE_URL", defaultBaseUrl(config.provider));
    config.apiKey = firstEnvValue("CPP_AI_API_KEY",
                                  "OPENAI_API_KEY",
                                  config.provider == kDeepSeekProvider ? "DEEPSEEK_API_KEY" :
                                  config.provider == kOpenRouterProvider ? "OPENROUTER_API_KEY" :
                                  config.provider == kGroqProvider ? "GROQ_API_KEY" :
                                  nullptr,
                                  "");
    config.apiKeyConfigured = !config.apiKey.empty();
    config.cacheCapacity = positiveEnvOrDefault("CPP_AI_CACHE_CAPACITY", 64);
    config.requestTimeoutSeconds = positiveEnvOrDefault("CPP_AI_TIMEOUT_SECONDS", 20);
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
            return AiChatResponse{false, "500 Internal Server Error", jsonError("tool registry is not configured"), false, false, "", 0};
        }

        ToolResult result = tools_->execute(request.tool);
        if (!result.ok)
        {
            return AiChatResponse{false, "400 Bad Request", jsonError(result.error), false, true, request.tool, 0};
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
        return AiChatResponse{true, "200 OK", body.str(), false, true, request.tool, 0};
    }

    const std::string key = cacheKey(request);
    std::string cachedReply;
    if (responseCache_.get(key, cachedReply))
    {
        return AiChatResponse{true, "200 OK", buildChatBody(request, cachedReply, true), true, false, "", 0};
    }

    if (shouldUseNetworkProvider())
    {
        const AiChatResponse networkResponse = callOpenAiCompatible(request);
        if (networkResponse.ok)
        {
            std::string reply;
            if (extractJsonStringField(networkResponse.body, "reply", &reply))
            {
                responseCache_.put(key, reply);
            }
        }
        return networkResponse;
    }

    const std::string reply = makeStubReply(request.message);
    responseCache_.put(key, reply);
    return AiChatResponse{true, "200 OK", buildChatBody(request, reply, false), false, false, "", 0};
}

std::string AiClient::configJson() const
{
    std::ostringstream body;
    body << "{"
         << "\"provider\":\"" << jsonEscape(config_.provider) << "\","
         << "\"model\":\"" << jsonEscape(config_.model) << "\","
         << "\"base_url\":\"" << jsonEscape(config_.baseUrl) << "\","
         << "\"api_key_configured\":" << (config_.apiKeyConfigured ? "true" : "false") << ","
         << "\"cache_capacity\":" << config_.cacheCapacity << ","
         << "\"timeout_seconds\":" << config_.requestTimeoutSeconds
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

AiChatResponse AiClient::callOpenAiCompatible(const AiChatRequest &request) const
{
    if (config_.baseUrl.empty())
    {
        return AiChatResponse{false, "500 Internal Server Error", jsonError("CPP_AI_BASE_URL is not configured"), false, false, "", 0};
    }

    ensureCurlGlobalInit();

    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        return AiChatResponse{false, "500 Internal Server Error", jsonError("failed to initialize curl"), false, false, "", 0};
    }

    const std::string url = stripTrailingSlash(config_.baseUrl) + "/chat/completions";
    const std::string payload = "{"
        "\"model\":\"" + jsonEscape(config_.model) + "\","
        "\"messages\":[{\"role\":\"user\",\"content\":\"" + jsonEscape(request.message) + "\"}],"
        "\"temperature\":0.2"
        "}";

    std::string response;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (config_.apiKeyConfigured)
    {
        const std::string auth = "Authorization: Bearer " + config_.apiKey;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCurlResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(config_.requestTimeoutSeconds));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "cpp-ai-service/0.1");

    const auto started = std::chrono::steady_clock::now();
    const CURLcode code = curl_easy_perform(curl);
    const auto finished = std::chrono::steady_clock::now();
    const long upstreamLatencyMs = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count());
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK)
    {
        std::ostringstream error;
        error << "{\"ok\":false,"
              << "\"error\":\"AI provider request failed: " << jsonEscape(curl_easy_strerror(code)) << "\","
              << "\"provider\":\"" << jsonEscape(config_.provider) << "\","
              << "\"base_url\":\"" << jsonEscape(config_.baseUrl) << "\""
              << "}";
        return AiChatResponse{false, "502 Bad Gateway", error.str(), false, false, "", upstreamLatencyMs};
    }

    if (httpStatus < 200 || httpStatus >= 300)
    {
        std::ostringstream error;
        error << "{\"ok\":false,"
              << "\"error\":\"provider returned HTTP " << httpStatus << "\","
              << "\"provider\":\"" << jsonEscape(config_.provider) << "\","
              << "\"body\":\"" << jsonEscape(response) << "\""
              << "}";
        return AiChatResponse{false, "502 Bad Gateway", error.str(), false, false, "", upstreamLatencyMs};
    }

    std::string reply;
    if (!extractJsonStringField(response, "content", &reply))
    {
        return AiChatResponse{false, "502 Bad Gateway", jsonError("provider response did not contain assistant content"), false, false, "", upstreamLatencyMs};
    }

    return AiChatResponse{true, "200 OK", buildChatBody(request, reply, false), false, false, "", upstreamLatencyMs};
}

bool AiClient::shouldUseNetworkProvider() const
{
    return config_.provider == kOpenAiProvider ||
           config_.provider == kOpenAiCompatibleProvider ||
           config_.provider == kDeepSeekProvider ||
           config_.provider == kOpenRouterProvider ||
           config_.provider == kGroqProvider ||
           config_.provider == kOllamaProvider;
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
