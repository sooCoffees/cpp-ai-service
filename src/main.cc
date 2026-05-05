#include <string>
#include <memory>

#include <TcpServer.h>
#include <Logger.h>
#include <sys/stat.h>
#include <libgen.h>
#include <signal.h>
#include <chrono>
#include <sstream>
#include "AiClient.h"
#include "AsyncLogging.h"
#include "HttpCodec.h"
#include "RagStore.h"
#include "ToolRegistry.h"
#include "WebPages.h"
#include "memoryPool.h"
// 日志文件滚动大小为1MB (1*1024*1024 bytes)
static const off_t kRollSize = 1*1024*1024;

namespace
{
struct RouteResult
{
    std::string response;
    std::string status;
    bool cacheHit;
    bool toolUsed;
    std::string toolName;
    long upstreamLatencyMs;
};

RouteResult makeRouteResult(const std::string &response, const std::string &status)
{
    RouteResult result;
    result.response = response;
    result.status = status;
    result.cacheHit = false;
    result.toolUsed = false;
    result.upstreamLatencyMs = 0;
    return result;
}

RouteResult handleChatRequest(const HttpRequest &request, AiClient &aiClient)
{
    if (request.method != "POST")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/chat expects POST")),
                               "405 Method Not Allowed");
    }

    std::string message;
    if (!extractJsonStringField(request.body, "message", &message) || message.empty())
    {
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError("request body must contain a non-empty string field named message")),
                               "400 Bad Request");
    }

    std::string tool;
    extractJsonStringField(request.body, "tool", &tool);

    AiChatRequest chatRequest;
    chatRequest.message = message;
    chatRequest.tool = tool;

    const AiChatResponse chatResponse = aiClient.chat(chatRequest);
    RouteResult result = makeRouteResult(jsonResponse(chatResponse.status, chatResponse.body), chatResponse.status);
    result.cacheHit = chatResponse.cacheHit;
    result.toolUsed = chatResponse.toolUsed;
    result.toolName = chatResponse.toolName;
    result.upstreamLatencyMs = chatResponse.upstreamLatencyMs;
    return result;
}

RouteResult handleToolsRequest(const HttpRequest &request, const ToolRegistry &tools)
{
    if (request.method != "GET")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/tools expects GET")),
                               "405 Method Not Allowed");
    }

    return makeRouteResult(jsonResponse("200 OK", tools.listToolsJson()), "200 OK");
}

RouteResult handleMcpToolsRequest(const HttpRequest &request, const ToolRegistry &tools)
{
    if (request.method != "GET")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/mcp/tools expects GET")),
                               "405 Method Not Allowed");
    }

    return makeRouteResult(jsonResponse("200 OK", tools.listMcpToolsJson()), "200 OK");
}

RouteResult handleMcpCallRequest(const HttpRequest &request, const ToolRegistry &tools)
{
    if (request.method != "POST")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/mcp/call expects POST")),
                               "405 Method Not Allowed");
    }

    std::string tool;
    if (!extractJsonStringField(request.body, "tool", &tool) || tool.empty())
    {
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError("request body must contain a non-empty string field named tool")),
                               "400 Bad Request");
    }

    std::string input;
    if (!extractJsonStringField(request.body, "query", &input))
    {
        extractJsonStringField(request.body, "message", &input);
    }

    const ToolResult toolResult = tools.execute(tool, input);
    if (!toolResult.ok)
    {
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError(toolResult.error)), "400 Bad Request");
    }

    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"protocol\":\"mcp-like\","
         << "\"tool\":\"" << jsonEscape(tool) << "\","
         << "\"result\":" << toolResult.json
         << "}";

    RouteResult result = makeRouteResult(jsonResponse("200 OK", body.str()), "200 OK");
    result.toolUsed = true;
    result.toolName = tool;
    return result;
}

RouteResult handleRagIngestRequest(const HttpRequest &request, InMemoryRagStore &ragStore)
{
    if (request.method != "POST")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/rag/ingest expects POST")),
                               "405 Method Not Allowed");
    }

    std::string id;
    std::string title;
    std::string content;
    if (!extractJsonStringField(request.body, "id", &id) || id.empty() ||
        !extractJsonStringField(request.body, "title", &title) || title.empty() ||
        !extractJsonStringField(request.body, "content", &content) || content.empty())
    {
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError("request body must contain non-empty string fields id, title, and content")),
                               "400 Bad Request");
    }

    ragStore.upsert(RagDocument{id, title, content});

    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"id\":\"" << jsonEscape(id) << "\","
         << "\"title\":\"" << jsonEscape(title) << "\""
         << "}";
    return makeRouteResult(jsonResponse("200 OK", body.str()), "200 OK");
}

RouteResult handleHealthRequest(const AiClient &aiClient)
{
    return makeRouteResult(jsonResponse("200 OK", "{\"ok\":true,\"ai\":" + aiClient.configJson() + "}"), "200 OK");
}

RouteResult handleRequest(const HttpRequest &request, AiClient &aiClient, const ToolRegistry &tools, InMemoryRagStore &ragStore)
{
    if (request.path == "/chat")
    {
        return handleChatRequest(request, aiClient);
    }
    if (request.path == "/tools")
    {
        return handleToolsRequest(request, tools);
    }
    if (request.path == "/health")
    {
        return handleHealthRequest(aiClient);
    }
    if (request.path == "/mcp/tools")
    {
        return handleMcpToolsRequest(request, tools);
    }
    if (request.path == "/mcp/call")
    {
        return handleMcpCallRequest(request, tools);
    }
    if (request.path == "/rag/ingest")
    {
        return handleRagIngestRequest(request, ragStore);
    }
    if (request.path == "/direct")
    {
        return makeRouteResult(renderDirectApiPage(), "200 OK");
    }
    if (request.path == "/")
    {
        return makeRouteResult(renderHomePage(), "200 OK");
    }

    return makeRouteResult(jsonResponse("404 Not Found", jsonError("route not found")), "404 Not Found");
}

RouteResult handleRawRequest(const std::string &raw, AiClient &aiClient, const ToolRegistry &tools, InMemoryRagStore &ragStore, HttpRequest *parsedRequest)
{
    HttpRequest request;
    if (!parseHttpRequest(raw, &request))
    {
        if (parsedRequest != nullptr)
        {
            *parsedRequest = request;
        }
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError("malformed HTTP request")),
                               "400 Bad Request");
    }

    if (parsedRequest != nullptr)
    {
        *parsedRequest = request;
    }

    return handleRequest(request, aiClient, tools, ragStore);
}

}

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
        , ragStore_(new InMemoryRagStore())
        , tools_(ToolRegistry::createDefault(ragStore_))
        , aiConfig_(AiClientConfig::fromEnvironment())
        , aiClient_(&tools_, aiConfig_)
    {
        ragStore_->seedDefaults();

        if (aiConfig_.provider != "stub" && aiConfig_.provider != "ollama" && !aiConfig_.apiKeyConfigured)
        {
            LOG_WARN << "AI provider configured as " << aiConfig_.provider.c_str()
                     << " but CPP_AI_API_KEY or provider API key is not set";
        }

        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的subloop线程数量
        server_.setThreadNum(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->connected())
        {
            LOG_INFO<<"Connection UP :"<<conn->peerAddress().toIpPort().c_str();
        }
        else
        {
            LOG_INFO<<"Connection DOWN :"<<conn->peerAddress().toIpPort().c_str();
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        const std::string request = buf->retrieveAllAsString();
        HttpRequest parsedRequest;
        const auto started = std::chrono::steady_clock::now();
        const RouteResult routeResult = handleRawRequest(request, aiClient_, tools_, *ragStore_, &parsedRequest);
        const auto finished = std::chrono::steady_clock::now();
        const long latencyMs = static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count());

        LOG_INFO << "request method=" << parsedRequest.method.c_str()
                 << " path=" << parsedRequest.path.c_str()
                 << " status=" << routeResult.status.c_str()
                 << " body_size=" << parsedRequest.body.size()
                 << " cache_hit=" << (routeResult.cacheHit ? "true" : "false")
                 << " tool_used=" << (routeResult.toolUsed ? "true" : "false")
                 << " tool=" << routeResult.toolName.c_str()
                 << " upstream_ms=" << routeResult.upstreamLatencyMs
                 << " total_ms=" << latencyMs;

        conn->send(routeResult.response);
        conn->shutdown();   // 关闭写端，HTTP/1.0 风格一请求一响应
    }
    TcpServer server_;
    EventLoop *loop_;
    std::shared_ptr<InMemoryRagStore> ragStore_;
    ToolRegistry tools_;
    AiClientConfig aiConfig_;
    AiClient aiClient_;

};
AsyncLogging* g_asyncLog = NULL;
AsyncLogging * getAsyncLog(){
    return g_asyncLog;
}
 void asyncLog(const char* msg, int len)
{
    AsyncLogging* logging = getAsyncLog();
    if (logging)
    {
        logging->append(msg, len);
    }
}
int main(int argc,char *argv[]) {
    // Browser clients may close an HTTP connection before the server finishes
    // writing. Ignore SIGPIPE so one closed socket does not kill the process.
    ::signal(SIGPIPE, SIG_IGN);

    //第一步启动日志，双缓冲异步写入磁盘.
    //创建一个文件夹
    const std::string LogDir="logs";
    mkdir(LogDir.c_str(),0755);
    //使用std::stringstream 构建日志文件夹
    std::ostringstream LogfilePath;
    LogfilePath << LogDir << "/" << ::basename(argv[0]); // 完整的日志文件路径
    AsyncLogging log(LogfilePath.str(), kRollSize);
    g_asyncLog = &log;
    Logger::setOutput(asyncLog); // 为Logger设置输出回调, 重新配接输出位置
    log.start(); // 开启日志后端线程
    //第二步启动内存池和LFU缓存
     // 初始化内存池
    memoryPool::HashBucket::initMemoryPool();

    //第三步启动底层网络模块
    EventLoop loop;
    InetAddress addr(8080);
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
 // 主loop开始事件循环  epoll_wait阻塞 等待就绪事件(主loop只注册了监听套接字的fd，所以只会处理新连接事件)
    std::cout << "================================================Start Web Server================================================" << std::endl;
    loop.loop();
    std::cout << "================================================Stop Web Server=================================================" << std::endl;
    //结束日志打印
    log.stop();
}
