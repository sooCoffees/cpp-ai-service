#include <string>

#include <TcpServer.h>
#include <Logger.h>
#include <sys/stat.h>
#include <libgen.h>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>
#include "AiClient.h"
#include "AsyncLogging.h"
#include "LFU.h"
#include "ToolRegistry.h"
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

std::string handleChatRequest(const HttpRequest &request, const AiClient &aiClient)
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

    std::string tool;
    extractJsonStringField(request.body, "tool", &tool);

    AiChatRequest chatRequest;
    chatRequest.message = message;
    chatRequest.tool = tool;

    const AiChatResponse chatResponse = aiClient.chat(chatRequest);
    return jsonResponse(chatResponse.status, chatResponse.body);
}

std::string handleHomeRequest()
{
    const std::string body = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>cpp-ai-service</title>
<style>
:root {
  color-scheme: dark;
  --bg: #111312;
  --panel: #181b1a;
  --panel-2: #202422;
  --border: #303633;
  --text: #f3f5f2;
  --muted: #9aa39d;
  --accent: #19c37d;
  --danger: #ff6b6b;
  --shadow: 0 18px 60px rgba(0, 0, 0, 0.28);
}
* { box-sizing: border-box; }
html, body { height: 100%; }
body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}
button, textarea, select { font: inherit; }
.app {
  min-height: 100vh;
  display: grid;
  grid-template-columns: 280px minmax(0, 1fr);
}
.sidebar {
  border-right: 1px solid var(--border);
  background: #151716;
  padding: 18px 14px;
  display: flex;
  flex-direction: column;
  gap: 18px;
}
.brand {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 4px 6px;
  font-weight: 700;
}
.logo {
  width: 34px;
  height: 34px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  background: var(--accent);
  color: #07110c;
  font-weight: 900;
}
.new-chat, .send {
  border: 0;
  cursor: pointer;
  border-radius: 8px;
  background: var(--text);
  color: #101211;
  font-weight: 700;
}
.new-chat {
  width: 100%;
  min-height: 42px;
}
.side-block {
  border-top: 1px solid var(--border);
  padding-top: 16px;
}
.side-label {
  margin: 0 0 8px;
  color: var(--muted);
  font-size: 12px;
  text-transform: uppercase;
  letter-spacing: 0;
}
select {
  width: 100%;
  min-height: 40px;
  color: var(--text);
  background: var(--panel-2);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 0 10px;
}
.hint {
  margin: 8px 0 0;
  color: var(--muted);
  font-size: 13px;
  line-height: 1.45;
}
.main {
  min-width: 0;
  display: grid;
  grid-template-rows: auto minmax(0, 1fr) auto;
}
.topbar {
  min-height: 60px;
  border-bottom: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 0 22px;
}
.title {
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.title strong { font-size: 15px; }
.status {
  color: var(--muted);
  font-size: 13px;
}
.status.ok { color: var(--accent); }
.status.error { color: var(--danger); }
.messages {
  overflow-y: auto;
  padding: 28px 18px;
}
.message {
  width: min(860px, 100%);
  margin: 0 auto 18px;
  display: grid;
  grid-template-columns: 36px minmax(0, 1fr);
  gap: 14px;
}
.avatar {
  width: 36px;
  height: 36px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  background: var(--panel-2);
  color: var(--muted);
  font-size: 13px;
  font-weight: 800;
}
.assistant .avatar {
  background: rgba(25, 195, 125, 0.18);
  color: var(--accent);
}
.bubble {
  min-width: 0;
  padding: 10px 0;
  line-height: 1.65;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}
.assistant .bubble {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 14px 16px;
  box-shadow: var(--shadow);
}
.composer {
  border-top: 1px solid var(--border);
  padding: 16px 18px 22px;
  background: rgba(17, 19, 18, 0.92);
}
.composer-inner {
  width: min(860px, 100%);
  margin: 0 auto;
  display: grid;
  grid-template-columns: minmax(0, 1fr) 48px;
  gap: 10px;
  align-items: end;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 10px;
}
textarea {
  width: 100%;
  min-height: 48px;
  max-height: 180px;
  resize: none;
  border: 0;
  outline: 0;
  color: var(--text);
  background: transparent;
  line-height: 1.5;
  padding: 11px 8px;
}
.send {
  width: 48px;
  height: 48px;
  display: grid;
  place-items: center;
  font-size: 20px;
}
.send:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}
@media (max-width: 760px) {
  .app { grid-template-columns: 1fr; }
  .sidebar {
    border-right: 0;
    border-bottom: 1px solid var(--border);
    padding: 12px;
  }
  .side-block { display: none; }
  .topbar { padding: 0 14px; }
  .messages { padding: 20px 14px; }
  .message {
    grid-template-columns: 32px minmax(0, 1fr);
    gap: 10px;
  }
  .avatar {
    width: 32px;
    height: 32px;
  }
}
</style>
</head>
<body>
<div class="app">
  <aside class="sidebar">
    <div class="brand"><div class="logo">AI</div><span>cpp-ai-service</span></div>
    <button class="new-chat" id="newChat">New chat</button>
    <div class="side-block">
      <p class="side-label">Tool</p>
      <select id="toolSelect">
        <option value="">No tool</option>
      </select>
      <p class="hint">Tools are served by the local C++ ToolRegistry through /tools.</p>
    </div>
    <div class="side-block">
      <p class="side-label">Gateway</p>
      <p class="hint">This UI posts to /chat. The current backend still returns a stub AI reply unless a local tool is selected.</p>
    </div>
  </aside>
  <main class="main">
    <header class="topbar">
      <div class="title">
        <strong>Chat</strong>
        <span class="status" id="status">Checking service...</span>
      </div>
    </header>
    <section class="messages" id="messages"></section>
    <form class="composer" id="chatForm">
      <div class="composer-inner">
        <textarea id="messageInput" rows="1" placeholder="Message cpp-ai-service"></textarea>
        <button class="send" id="sendButton" type="submit" title="Send">↑</button>
      </div>
    </form>
  </main>
</div>
<script>
const messages = document.getElementById('messages');
const form = document.getElementById('chatForm');
const input = document.getElementById('messageInput');
const sendButton = document.getElementById('sendButton');
const statusEl = document.getElementById('status');
const toolSelect = document.getElementById('toolSelect');
const newChat = document.getElementById('newChat');

function setStatus(text, state) {
  statusEl.textContent = text;
  statusEl.className = 'status' + (state ? ' ' + state : '');
}

function addMessage(role, text) {
  const item = document.createElement('article');
  item.className = 'message ' + role;
  const avatar = document.createElement('div');
  avatar.className = 'avatar';
  avatar.textContent = role === 'user' ? 'You' : 'AI';
  const bubble = document.createElement('div');
  bubble.className = 'bubble';
  bubble.textContent = text;
  item.appendChild(avatar);
  item.appendChild(bubble);
  messages.appendChild(item);
  messages.scrollTop = messages.scrollHeight;
  return bubble;
}

function resetChat() {
  messages.innerHTML = '';
  addMessage('assistant', 'Hi, I am cpp-ai-service. Send a message to test the C++ /chat gateway, or select a local tool from the sidebar.');
  input.focus();
}

function resizeInput() {
  input.style.height = 'auto';
  input.style.height = Math.min(input.scrollHeight, 180) + 'px';
}

async function loadHealth() {
  try {
    const res = await fetch('/health');
    if (!res.ok) throw new Error('HTTP ' + res.status);
    setStatus('Service online', 'ok');
  } catch (err) {
    setStatus('Service unavailable', 'error');
  }
}

async function loadTools() {
  try {
    const res = await fetch('/tools');
    const data = await res.json();
    if (!data.ok || !Array.isArray(data.tools)) return;
    data.tools.forEach((tool) => {
      const option = document.createElement('option');
      option.value = tool.name;
      option.textContent = tool.name;
      option.title = tool.description || '';
      toolSelect.appendChild(option);
    });
  } catch (err) {
    // Tools are optional for the UI.
  }
}

async function sendMessage(text) {
  const payload = { message: text };
  if (toolSelect.value) payload.tool = toolSelect.value;

  const res = await fetch('/chat', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  const data = await res.json();
  if (!res.ok || data.ok === false) {
    throw new Error(data.error || ('HTTP ' + res.status));
  }

  let reply = data.reply || '';
  if (data.tool_used && data.tool_result) {
    reply += '\n\nTool result:\n' + JSON.stringify(data.tool_result, null, 2);
  }
  return reply || '(empty response)';
}

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  const text = input.value.trim();
  if (!text) return;

  addMessage('user', text);
  input.value = '';
  resizeInput();
  input.disabled = true;
  sendButton.disabled = true;
  setStatus('Thinking...');
  const pending = addMessage('assistant', '...');

  try {
    pending.textContent = await sendMessage(text);
    setStatus('Service online', 'ok');
  } catch (err) {
    pending.textContent = 'Request failed: ' + err.message;
    setStatus('Request failed', 'error');
  } finally {
    input.disabled = false;
    sendButton.disabled = false;
    input.focus();
  }
});

input.addEventListener('input', resizeInput);
input.addEventListener('keydown', (event) => {
  if (event.key === 'Enter' && !event.shiftKey) {
    event.preventDefault();
    form.requestSubmit();
  }
});
newChat.addEventListener('click', resetChat);

resetChat();
resizeInput();
loadHealth();
loadTools();
</script>
</body>
</html>)HTML";
    return httpResponse("200 OK", "text/html; charset=utf-8", body);
}

std::string handleToolsRequest(const HttpRequest &request, const ToolRegistry &tools)
{
    if (request.method != "GET")
    {
        return jsonResponse("405 Method Not Allowed", jsonError("/tools expects GET"));
    }

    return jsonResponse("200 OK", tools.listToolsJson());
}

std::string handleRequest(const HttpRequest &request, const AiClient &aiClient, const ToolRegistry &tools)
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
        return jsonResponse("200 OK", "{\"ok\":true}");
    }
    if (request.path == "/")
    {
        return handleHomeRequest();
    }

    return jsonResponse("404 Not Found", jsonError("route not found"));
}

std::string handleRawRequest(const std::string &raw, const AiClient &aiClient, const ToolRegistry &tools)
{
    HttpRequest request;
    if (!parseHttpRequest(raw, &request))
    {
        return jsonResponse("400 Bad Request", jsonError("malformed HTTP request"));
    }

    return handleRequest(request, aiClient, tools);
}

}

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
        , tools_(ToolRegistry::createDefault())
        , aiClient_(&tools_)
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
        const std::string response = handleRawRequest(request, aiClient_, tools_);

        conn->send(response);
        conn->shutdown();   // 关闭写端，HTTP/1.0 风格一请求一响应
    }
    TcpServer server_;
    EventLoop *loop_;
    ToolRegistry tools_;
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
