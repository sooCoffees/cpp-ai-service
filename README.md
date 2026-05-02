# cpp-ai-service

`cpp-ai-service` is a C++ AI service gateway built on a custom Reactor-style TCP server.

The project is not meant to be just a thin chatbot wrapper. Its focus is the backend runtime around AI requests: networking, event dispatch, connection management, async logging, memory management, local tools, and a path toward cache-backed and MCP-like AI service behavior.

## Current Features

- C++11 custom web server runtime
- Reactor-style event loop
- `TcpServer` / `TcpConnection` connection lifecycle
- `Channel` + `Poller` event dispatch
- Linux `epoll` support
- macOS `poll(2)` fallback
- async logging module
- memory pool module
- LFU cache module available for future response caching
- HTTP request parsing for method, path, version, headers, `Content-Length`, and body
- JSON response helpers
- `/health` endpoint
- `/chat` endpoint
- `AiClient` boundary for chat response generation
- `ToolRegistry` for local tools
- `/tools` endpoint for discovering local tools
- `/chat` can call local tools through a request field

Current built-in tools:

- `project_status`
- `server_time`

## Architecture

High-level request flow:

```text
client
  -> TcpServer
  -> Acceptor
  -> TcpConnection
  -> Buffer
  -> HTTP parser
  -> route handler
  -> AiClient
  -> optional ToolRegistry
  -> HTTP JSON response
```

Event dispatch flow:

```text
EventLoop::loop()
  -> Poller::poll()
     -> Linux: epoll_wait
     -> macOS: poll
  -> Channel::handleEvent()
  -> TcpConnection::handleRead()
  -> EchoServer::onMessage()
```

The current AI layer is still a stub. The useful part is the service boundary:

```text
/chat
  -> parse JSON message
  -> AiClient::chat()
  -> optional local tool execution
  -> structured JSON response
```

This makes it easier to replace the stub with a real model provider later without rewriting the HTTP and networking layer.

## Project Structure

```text
cpp-ai-service/
├── CMakeLists.txt          # top-level build configuration
├── Dockerfile              # container setup
├── README.md               # project documentation
├── include/                # public headers
│   ├── AiClient.h          # AI reply boundary
│   ├── ToolRegistry.h      # local tool registry
│   ├── EventLoop.h         # event loop abstraction
│   ├── Channel.h           # fd + callback wrapper
│   ├── Poller.h            # IO multiplexer interface
│   ├── EPollPoller.h       # Linux epoll implementation
│   ├── PollPoller.h        # macOS poll implementation
│   ├── TcpServer.h         # server abstraction
│   ├── TcpConnection.h     # connection abstraction
│   ├── Buffer.h            # input/output buffer
│   ├── LFU.h               # LFU cache
│   └── memoryPool.h        # memory pool
├── src/                    # server and gateway implementation
│   ├── main.cc             # current HTTP routes and service startup
│   ├── AiClient.cc         # chat response generation
│   ├── ToolRegistry.cc     # built-in local tools
│   ├── EventLoop.cc        # event loop implementation
│   ├── Channel.cc          # event dispatch implementation
│   ├── EPollPoller.cc      # Linux epoll poller
│   ├── PollPoller.cc       # macOS poll poller
│   ├── TcpServer.cc        # server lifecycle
│   ├── TcpConnection.cc    # connection IO
│   ├── Acceptor.cc         # accepts new connections
│   ├── Socket.cc           # socket operations
│   └── Buffer.cc           # buffer operations
├── log/                    # async logging module
└── memory/                 # memory pool module
```

## Requirements

Recommended:

- macOS with AppleClang, or Linux with GCC/Clang
- CMake 3.10+
- C++11 compiler

The original network stack was Linux-oriented. The current code includes macOS compatibility for APIs such as `epoll`, `eventfd`, `timerfd`, `accept4`, `sendfile`, and thread id handling.

## Build

```bash
cmake -S . -B build
cmake --build build -j 4
```

The executable is generated at:

```text
bin/main
```

## Run

```bash
./bin/main
```

Expected startup output:

```text
================================================Start Web Server================================================
```

The service listens on:

```text
http://127.0.0.1:8080
```

If the port is already in use:

```bash
lsof -nP -iTCP:8080
kill <PID>
```

## API

### `GET /`

Simple browser page for confirming that the server is running.

```bash
curl -i --max-time 3 http://127.0.0.1:8080/
```

### `GET /health`

Health check.

```bash
curl -i --max-time 3 http://127.0.0.1:8080/health
```

Response:

```json
{"ok":true}
```

### `GET /tools`

List local tools.

```bash
curl -i --max-time 3 http://127.0.0.1:8080/tools
```

Example response:

```json
{
  "ok": true,
  "tools": [
    {
      "name": "project_status",
      "description": "Return the current cpp-ai-service project status."
    },
    {
      "name": "server_time",
      "description": "Return the server timestamp."
    }
  ]
}
```

### `POST /chat`

Send a chat message.

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"hello from mac"}'
```

Example response:

```json
{
  "ok": true,
  "message": "hello from mac",
  "reply": "Stub AI reply for: hello from mac",
  "tool_used": false
}
```

### `POST /chat` With a Tool

Call a local tool through `/chat`.

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"show project status","tool":"project_status"}'
```

Example response:

```json
{
  "ok": true,
  "message": "show project status",
  "reply": "Tool project_status executed for: show project status",
  "tool_used": true,
  "tool": "project_status",
  "tool_result": {
    "project": "cpp-ai-service",
    "status": "C++ AI service gateway skeleton"
  }
}
```

## Current Limitations

- `/chat` still returns a stub AI reply.
- There is no real model provider integration yet.
- JSON parsing is intentionally minimal and currently targets simple request bodies like `{"message":"..."}`.
- HTTP parsing is still inside `src/main.cc` and should be split into dedicated request/response helpers.
- Tool execution is local and manually selected by request field; there is no model-driven tool-call loop yet.
- LFU cache exists but is not yet connected to `/chat`.
- MCP compatibility is planned but not implemented yet.

## Roadmap

Near-term:

- Move HTTP parsing and response formatting out of `main.cc`
- Add LFU prompt/response cache
- Read model provider configuration from environment variables
- Add real AI provider support with an HTTP client such as `libcurl`
- Add request logging for method, path, status, cache hit, tool name, body size, and upstream latency

Later:

- Add browser chat UI
- Add model-driven tool calling
- Add MCP-like tool discovery and execution endpoint
- Move toward MCP JSON-RPC compatibility

## Development Notes

Before committing:

```bash
git status --short --branch
git diff --stat
cmake --build build -j 4
```

Do not commit generated outputs or secrets:

- `build/`
- `bin/`
- `lib/`
- logs
- `.env`
- API keys
- local IDE files

## License

GPL-3.0. See [LICENSE](LICENSE).
