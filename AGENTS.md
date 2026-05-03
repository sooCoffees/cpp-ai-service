# AGENTS.md

本文件是本项目给 AI 助手和开发协作者看的项目上下文。以后在修改代码、解释架构、规划任务之前，先读这个文件。

## 项目一句话定位

`cpp-ai-service` 不是普通的聊天机器人项目，而是一个基于 C++ 自研网络服务器的 AI Service Gateway。

当前目标是：

```text
C++ Reactor WebServer
  -> HTTP /chat API
  -> AiClient 边界
  -> ToolRegistry 本地工具层
  -> LFU prompt/response cache
  -> MCP-like / MCP-compatible 工具接口
```

短期不要把项目做成简单的“AI 套壳”。真正有区分度的地方是：用 C++ 网络服务端、异步日志、内存池、缓存、工具调用和 MCP 方向，做一个更偏系统工程的 AI 服务网关。

## 当前项目状态

已经完成：

- CMake 项目结构。
- C++11 构建。
- `TcpServer` / `TcpConnection` / `EventLoop` / `Channel` / `Poller` 网络框架。
- Linux 下 `EPollPoller`。
- macOS 下 `PollPoller`，用 `poll(2)` 替代 `epoll`。
- macOS 兼容：
  - `eventfd` fallback 为 pipe
  - `accept4` fallback 为 `accept + fcntl`
  - `timerfd` fallback 为 pipe-based placeholder
  - Linux `sendfile` 和 macOS `sendfile` 分支
  - `SYS_gettid` fallback 为 `pthread_threadid_np`
  - `fwrite_unlocked` fallback 为 `fwrite`
- `/health` endpoint。
- `/chat` endpoint。
- 基础 HTTP request 解析：
  - method
  - path
  - version
  - headers
  - `Content-Length`
  - body
- 简单 JSON 字段解析：当前只解析 string 类型的 `"message"`。
- `/chat` 错误处理：
  - `GET /chat` 返回 `405`
  - 缺少 `message` 返回 `400`
  - 未知路径返回 `404`
- `AiClient` 已经从 `main.cc` 抽出。
- `ToolRegistry` 已经接入，包含 `project_status` 和 `server_time`。
- `/tools` 已经可列出工具。
- `/chat` 已经支持通过 `"tool"` 字段调用本地工具。
- LFU response cache 已经接入普通非工具 `/chat` 回复，重复消息会返回 `cache_hit:true`。
- `CPP_AI_PROVIDER`、`CPP_AI_MODEL`、`CPP_AI_CACHE_CAPACITY` 和 `OPENAI_API_KEY` 的配置读取已经接入。
- `LEARNING_PLAN.md` 已更新为 AI Gateway + tools/cache/MCP 路线。

还没完成：

- 真实模型 API 调用。
- MCP-like endpoint。
- 正式 MCP JSON-RPC 兼容。
- HTTP 解析和响应代码从 `main.cc` 拆分。
- 浏览器聊天 UI。

## 重要文件和目录

```text
CMakeLists.txt
```

顶层 CMake。设置 C++11、头文件目录、输出目录，并添加 `src`、`memory`、`log` 子目录。

```text
src/main.cc
```

当前服务入口。现在包含：

- HTTP request parser
- JSON helper
- `/`
- `/health`
- `/chat`
- `EchoServer`
- 日志启动
- 内存池初始化
- LFU cache 初始化
- 监听 `8080`

注意：这里已经开始变胖。后续应把 HTTP、AI、tools、cache 逻辑拆出去。

```text
include/
```

所有头文件。

核心头文件：

- `EventLoop.h`
- `Channel.h`
- `Poller.h`
- `EPollPoller.h`
- `PollPoller.h`
- `TcpServer.h`
- `TcpConnection.h`
- `Buffer.h`
- `LFU.h`
- `memoryPool.h`

```text
src/
```

网络框架和服务入口实现。

重点文件：

- `EventLoop.cc`
- `Channel.cc`
- `Poller.cc`
- `EPollPoller.cc`
- `PollPoller.cc`
- `TcpServer.cc`
- `TcpConnection.cc`
- `Acceptor.cc`
- `Socket.cc`
- `Buffer.cc`
- `main.cc`

```text
log/
```

异步日志模块。

```text
memory/
```

内存池模块。

```text
LEARNING_PLAN.md
```

项目学习和开发路线。当前路线是 AI Service Gateway，不是普通 chatbot wrapper。

```text
memory.md
habit.md
terminology.md
```

项目长期上下文文件：

- `memory.md`：记录踩坑、设计决策、项目经验。
- `habit.md`：记录常用命令和工作流。
- `terminology.md`：记录术语解释。

## 当前运行方式

构建：

```bash
cmake -S . -B build
cmake --build build -j 4
```

运行：

```bash
./bin/main
```

成功启动时会看到：

```text
================================================Start Web Server================================================
```

默认监听：

```text
127.0.0.1:8080
```

如果端口被占用：

```bash
lsof -nP -iTCP:8080
kill <PID>
```

## 当前接口

### `GET /`

浏览器首页，用来确认当前跑的是新版本。

预期页面包含：

```text
cpp-ai-service
POST /chat with JSON {"message":"..."}.
```

如果看到：

```text
It works
Minimal HTTP response.
```

说明跑的是旧版本或旧进程。

### `GET /health`

命令：

```bash
curl -i --max-time 3 http://127.0.0.1:8080/health
```

预期：

```json
{"ok":true}
```

### `POST /chat`

命令：

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"hello from mac"}'
```

预期：

```json
{"ok":true,"message":"hello from mac","reply":"Stub AI reply for: hello from mac"}
```

### `GET /chat`

浏览器地址栏访问 `/chat` 会发 `GET`，这是错误用法。

预期返回：

```json
{"ok":false,"error":"/chat expects POST"}
```

这是正确行为，不是 bug。

## 架构理解

### Reactor 主线

核心事件链路：

```text
EventLoop::loop()
  -> Poller::poll()
     -> Linux: epoll_wait
     -> macOS: poll
  -> activeChannels
  -> Channel::handleEvent()
  -> TcpConnection::handleRead()
  -> EchoServer::onMessage()
  -> handleRawRequest()
  -> handleRequest()
  -> conn->send(response)
```

### 角色划分

`EventLoop`

- 一个线程一个事件循环。
- 等待 IO 事件。
- 分发 active `Channel`。
- 执行跨线程 pending functors。

`Channel`

- 绑定 fd 和回调。
- 根据读、写、关闭、错误事件调用对应 callback。

`Poller`

- IO 多路复用抽象。
- Linux 默认用 `EPollPoller`。
- macOS 用 `PollPoller`。

`TcpServer`

- 对外服务器抽象。
- 持有 `Acceptor`。
- 管理连接 map。
- 分配连接到 sub loop。

`TcpConnection`

- 表示一个客户端连接。
- 拥有 socket、channel、input buffer、output buffer。
- 负责读、写、关闭和回调用户逻辑。

`main.cc`

- 当前业务入口。
- 以后应该逐步瘦身。

## macOS 兼容注意事项

这个项目最初偏 Linux/muduo 风格。macOS 没有一些 Linux API：

- `epoll`
- `eventfd`
- `timerfd`
- `accept4`
- Linux 版本 `sendfile`
- `SYS_gettid`
- `fwrite_unlocked`

现在已经做了平台分支。以后改底层网络代码时，不要只测 Linux，也要确认 macOS 能编译。

在受限沙箱里运行 `./bin/main` 可能 bind 失败：

```text
bind sockfd fail errno=1
```

这通常是环境限制，不一定是代码错误。在普通 macOS Terminal 中运行即可。

## Git 状态和提交规则

提交前必须看：

```bash
git status --short --branch
git diff --stat
cmake --build build -j 4
```

不要提交：

- `build/`
- `bin/`
- `lib/`
- `.env`
- API key
- 临时日志
- IDE 私有配置
- 大型生成文件

当前 `.gitignore` 应忽略：

- build outputs
- logs
- `.vscode`
- `.DS_Store`

`LEARNING_PLAN.md` 应该被 Git 跟踪，不要再忽略它。

如果 push 被拒绝：

```bash
git pull --rebase origin main
git push origin main
```

## 开发风格要求

用户是为了学习，不只是要结果。

回答和改代码时应遵守：

1. 先解释原理，再给步骤。
2. 说明为什么这么设计。
3. 保持代码清晰，不要过早抽象。
4. 每次改动尽量小而可运行。
5. 优先让一个功能闭环跑通。
6. 修改后尽量给出 `curl` 或构建命令验证。
7. 遇到框架、库、架构选择时，要比较优缺点再推荐。

## 下一步路线

按当前计划，下一步应该做：

```text
5/5: libcurl 调真实 AI provider
5/8: MCP-like tool endpoint
5/9: MCP compatibility roadmap
```

最合理的下一步代码任务：

```text
接入真实 provider 调用边界，优先考虑 libcurl。
```

目标是在不破坏现有 `/chat`、工具调用和缓存行为的前提下，把 stub reply 替换为可配置 provider 的真实回复。

## 重要提醒

- 当前 `/chat` 还不是真 AI，只是 stub。
- 当前 JSON parser 是临时实现，只适合简单 `{"message":"..."}`。
- 后续复杂 JSON 应考虑引入 JSON 库，例如 `nlohmann/json`。
- 后续真实 AI 调用建议用 `libcurl`，不要手写 HTTPS。
- 项目亮点应放在 C++ gateway、tools、cache、MCP-like 能力，而不是普通聊天页面。
