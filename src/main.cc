#include <string>

#include <TcpServer.h>
#include <Logger.h>
#include <sys/stat.h>
#include <libgen.h>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>
#include "AsyncLogging.h"
#include "LFU.h"
#include "memoryPool.h"
// 日志文件滚动大小为1MB (1*1024*1024 bytes)
static const off_t kRollSize = 1*1024*1024;

namespace
{
struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string toLower(std::string value)
{
    for (char &ch : value)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string trim(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return value.substr(begin, end - begin);
}

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

bool parseHttpRequest(const std::string &raw, HttpRequest *request)
{
    const size_t requestLineEnd = raw.find("\r\n");
    if (requestLineEnd == std::string::npos)
    {
        return false;
    }

    std::istringstream requestLine(raw.substr(0, requestLineEnd));
    if (!(requestLine >> request->method >> request->path >> request->version))
    {
        return false;
    }

    const size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        return false;
    }

    size_t lineStart = requestLineEnd + 2;
    while (lineStart < headerEnd)
    {
        const size_t lineEnd = raw.find("\r\n", lineStart);
        if (lineEnd == std::string::npos || lineEnd > headerEnd)
        {
            return false;
        }

        const std::string line = raw.substr(lineStart, lineEnd - lineStart);
        const size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            request->headers[toLower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
        }
        lineStart = lineEnd + 2;
    }

    request->body = raw.substr(headerEnd + 4);
    auto contentLength = request->headers.find("content-length");
    if (contentLength != request->headers.end())
    {
        const size_t expected = static_cast<size_t>(std::strtoul(contentLength->second.c_str(), nullptr, 10));
        if (request->body.size() > expected)
        {
            request->body.resize(expected);
        }
    }

    return true;
}

bool extractJsonStringField(const std::string &json, const std::string &field, std::string *value)
{
    const std::string key = "\"" + field + "\"";
    const size_t keyPos = json.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const size_t colon = json.find(':', keyPos + key.size());
    if (colon == std::string::npos)
    {
        return false;
    }

    size_t pos = colon + 1;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
    {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '"')
    {
        return false;
    }

    ++pos;
    std::string parsed;
    while (pos < json.size())
    {
        char ch = json[pos++];
        if (ch == '"')
        {
            *value = parsed;
            return true;
        }
        if (ch == '\\' && pos < json.size())
        {
            char escaped = json[pos++];
            switch (escaped)
            {
            case '"':
            case '\\':
            case '/':
                parsed += escaped;
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
                parsed += escaped;
                break;
            }
        }
        else
        {
            parsed += ch;
        }
    }

    return false;
}

std::string jsonError(const std::string &message)
{
    return "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
}

std::string httpResponse(const std::string &status,
                         const std::string &contentType,
                         const std::string &body)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string jsonResponse(const std::string &status, const std::string &body)
{
    return httpResponse(status, "application/json; charset=utf-8", body);
}

std::string handleChatRequest(const HttpRequest &request)
{
    if (request.method != "POST")
    {
        return jsonResponse("405 Method Not Allowed", jsonError("/chat expects POST"));
    }

    std::string message;
    if (!extractJsonStringField(request.body, "message", &message) || message.empty())
    {
        return jsonResponse("400 Bad Request", jsonError("request body must contain a non-empty string field named message"));
    }

    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"message\":\"" << jsonEscape(message) << "\","
         << "\"reply\":\"Stub AI reply for: " << jsonEscape(message) << "\""
         << "}";
    return jsonResponse("200 OK", body.str());
}

std::string handleHomeRequest()
{
    const std::string body =
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<title>cpp-ai-service</title></head>"
        "<body><h1>cpp-ai-service</h1><p>POST /chat with JSON {\"message\":\"...\"}.</p></body></html>";
    return httpResponse("200 OK", "text/html; charset=utf-8", body);
}

std::string handleRequest(const HttpRequest &request)
{
    if (request.path == "/chat")
    {
        return handleChatRequest(request);
    }
    if (request.path == "/health")
    {
        return jsonResponse("200 OK", "{\"ok\":true}");
    }
    if (request.path == "/")
    {
        return handleHomeRequest();
    }

    return jsonResponse("404 Not Found", jsonError("route not found"));
}

std::string handleRawRequest(const std::string &raw)
{
    HttpRequest request;
    if (!parseHttpRequest(raw, &request))
    {
        return jsonResponse("400 Bad Request", jsonError("malformed HTTP request"));
    }

    return handleRequest(request);
}

}

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
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
        const std::string response = handleRawRequest(request);

        conn->send(response);
        conn->shutdown();   // 关闭写端，HTTP/1.0 风格一请求一响应
    }
    TcpServer server_;
    EventLoop *loop_;

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

    // 初始化缓存
    const int CAPACITY = 5;  
    KamaCache::KLfuCache<int, std::string> lfu(CAPACITY);
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
