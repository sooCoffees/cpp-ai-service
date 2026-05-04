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
- LFU cache module connected for repeated non-tool chat messages
- HTTP request parsing for method, path, version, headers, `Content-Length`, and body
- JSON response helpers
- `/health` endpoint
- `/chat` endpoint
- `AiClient` boundary for chat response generation
- `ToolRegistry` for local tools
- `/tools` endpoint for discovering local tools
- `/chat` can call local tools through a request field
- request metadata logging for method, path, status, cache hit, tool usage, body size, upstream latency, and total latency
- `HttpCodec` helper for HTTP request parsing and response formatting
- MCP-like tool discovery and invocation endpoints
- environment-based AI provider configuration without committing secrets

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
  -> HttpCodec
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
  -> LFU response cache for normal replies
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
│   ├── HttpCodec.h         # HTTP parsing/response helpers
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
│   ├── HttpCodec.cc        # HTTP request parsing and response formatting
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

Example response:

```json
{
  "ok": true,
  "ai": {
    "provider": "stub",
    "model": "stub-local",
    "base_url": "https://api.openai.com/v1",
    "api_key_configured": false,
    "cache_capacity": 64,
    "timeout_seconds": 20
  }
}
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
  "provider": "stub",
  "model": "stub-local",
  "cache_hit": false,
  "tool_used": false
}
```

Sending the same non-tool message again should return:

```json
"cache_hit": true
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
  "provider": "stub",
  "model": "stub-local",
  "cache_hit": false,
  "tool_used": true,
  "tool": "project_status",
  "tool_result": {
    "project": "cpp-ai-service",
    "status": "C++ AI service gateway skeleton"
  }
}
```

### `GET /mcp/tools`

List local tools through a stable MCP-like discovery shape.

```bash
curl -i --max-time 3 http://127.0.0.1:8080/mcp/tools
```

Example response:

```json
{
  "ok": true,
  "protocol": "mcp-like",
  "tools": [
    {
      "name": "project_status",
      "description": "Return the current cpp-ai-service project status.",
      "input_schema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    }
  ]
}
```

This endpoint is intentionally MCP-like, not full MCP JSON-RPC compatibility yet.

### `POST /mcp/call`

Invoke a local tool through the MCP-like endpoint.

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/mcp/call \
  -H 'Content-Type: application/json' \
  -d '{"tool":"server_time"}'
```

Example response:

```json
{
  "ok": true,
  "protocol": "mcp-like",
  "tool": "server_time",
  "result": {
    "timestamp": "2026/05/04 01:10:05.025517"
  }
}
```

## Request Logging

The gateway logs request metadata without dumping request bodies or API keys.

Each handled request logs:

- method
- path
- status
- request body size
- cache hit
- tool usage
- tool name
- upstream provider latency
- total local handling latency

Example log line:

```text
request method=POST path=/chat status=200 OK body_size=29 cache_hit=true tool_used=false tool= upstream_ms=0 total_ms=0
```

## Configuration

The gateway reads runtime configuration from environment variables:

```bash
CPP_AI_PROVIDER=stub
CPP_AI_MODEL=stub-local
CPP_AI_BASE_URL=https://api.openai.com/v1
CPP_AI_API_KEY=...
CPP_AI_CACHE_CAPACITY=64
CPP_AI_TIMEOUT_SECONDS=20
```

`CPP_AI_API_KEY` is the generic API key variable. The service also checks common provider variables such as `OPENAI_API_KEY`, `DEEPSEEK_API_KEY`, `OPENROUTER_API_KEY`, and `GROQ_API_KEY`. API keys are never printed in `/health`; only `api_key_configured` is exposed.

Stub mode:

```bash
CPP_AI_PROVIDER=stub CPP_AI_MODEL=stub-local ./bin/main
```

OpenAI-compatible mode:

```bash
CPP_AI_PROVIDER=openai_compatible \
CPP_AI_BASE_URL=https://api.deepseek.com/v1 \
CPP_AI_MODEL=deepseek-chat \
CPP_AI_API_KEY=... \
./bin/main
```

Provider shortcuts:

```bash
CPP_AI_PROVIDER=openai CPP_AI_MODEL=gpt-4.1-mini OPENAI_API_KEY=... ./bin/main
CPP_AI_PROVIDER=deepseek CPP_AI_MODEL=deepseek-chat DEEPSEEK_API_KEY=... ./bin/main
CPP_AI_PROVIDER=openrouter CPP_AI_MODEL=openai/gpt-4.1-mini OPENROUTER_API_KEY=... ./bin/main
CPP_AI_PROVIDER=groq CPP_AI_MODEL=llama-3.1-8b-instant GROQ_API_KEY=... ./bin/main
CPP_AI_PROVIDER=ollama CPP_AI_MODEL=llama3.2 ./bin/main
```

Supported network providers use an OpenAI-compatible `POST /chat/completions` API. Set `CPP_AI_BASE_URL` to point at any compatible host, including local services like LM Studio or Ollama.

If `CPP_AI_PROVIDER` is not `stub` or `ollama` and no API key is configured, the service logs a warning without exposing secrets.

## Current Limitations

- Real provider support currently targets OpenAI-compatible chat completion APIs.
- JSON parsing is intentionally minimal and currently targets simple request bodies like `{"message":"..."}`.
- `HttpCodec` now owns basic HTTP parsing/response formatting, but large HTML route handlers still live in `src/main.cc`.
- Tool execution is local and manually selected by request field; there is no model-driven tool-call loop yet.
- LFU cache is connected for repeated non-tool `/chat` messages, but cache invalidation and metrics are still basic.
- MCP-like JSON endpoints exist, but full MCP JSON-RPC compatibility is not implemented yet.

## Roadmap

Near-term:

- Continue moving route handlers and embedded HTML out of `main.cc`
- Add provider-specific request options for headers, organization/project IDs, and streaming
- Add request id tracing and richer latency metrics

Later:

- Add model-driven tool calling
- Move toward MCP JSON-RPC compatibility
- Add RAG design, retrieval tools, and optional vector search

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
