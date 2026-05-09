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
- demo local auth endpoints: `/auth/register`, `/auth/login`, and `/auth/me`
- `/chat` endpoint
- `AiClient` boundary for chat response generation
- `ToolRegistry` for local tools
- `/tools` endpoint for discovering local tools
- `/chat` can call local tools through a request field
- request metadata logging for method, path, status, cache hit, tool usage, body size, upstream latency, and total latency
- `HttpCodec` helper for HTTP request parsing and response formatting
- `WebPages` helper for embedded browser pages
- Customize main browser workspace with a compact rail, agent sidebar, top mode chips, and bottom composer
- main page login/register modal connected to demo local auth
- client-side agent cards on the main page, including temporary create/delete/select behavior
- Provider settings panel shown as a child panel under the selected agent card
- bottom composer tool selector for switching between direct provider mode and local gateway tool mode
- MCP-like tool discovery and invocation endpoints
- minimal in-memory RAG store with an embedding/vector-store boundary
- `rag_search` local retrieval tool
- `/rag/ingest` endpoint for adding temporary local documents
- `UserStore` boundary for demo users and in-memory sessions
- environment-based AI provider configuration without committing secrets
- `future_extension.md` roadmap for later application-level features such as persistent users, chat history, voice, image upload, MySQL persistence, MCP config files, and async jobs

Current built-in tools:

- `project_status`
- `server_time`
- `rag_search`

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
  -> optional UserStore
  -> AiClient
  -> optional ToolRegistry
  -> optional InMemoryRagStore
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
  -> optional rag_search retrieval
  -> structured JSON response
```

This makes it easier to replace the stub with a real model provider later without rewriting the HTTP and networking layer.

RAG flow:

```text
/rag/ingest
  -> InMemoryRagStore::upsert()

/chat + {"tool":"rag_search"}
  -> AiClient
  -> ToolRegistry
  -> InMemoryRagStore::search()
  -> matched context in tool_result
```

`EmbeddingProvider` and `VectorStore` are explicit boundaries. The current implementation uses a small keyword embedding provider and in-memory storage, but the `/chat` and `/mcp/call` contracts do not need to change when a real embedding API or vector database is added.

## Project Structure

```text
cpp-ai-service/
├── CMakeLists.txt          # top-level build configuration
├── Dockerfile              # container setup
├── README.md               # project documentation
├── future_extension.md     # future extension roadmap
├── include/                # public headers
│   ├── AiClient.h          # AI reply boundary
│   ├── HttpCodec.h         # HTTP parsing/response helpers
│   ├── RagStore.h          # RAG embedding and vector-store boundary
│   ├── ToolRegistry.h      # local tool registry
│   ├── UserStore.h         # demo local users and sessions
│   ├── WebPages.h          # browser page rendering helpers
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
│   ├── main.cc             # HTTP routes and service startup
│   ├── AiClient.cc         # chat response generation
│   ├── HttpCodec.cc        # HTTP request parsing and response formatting
│   ├── RagStore.cc         # minimal in-memory retrieval implementation
│   ├── ToolRegistry.cc     # built-in local tools
│   ├── UserStore.cc        # demo registration/login/session store
│   ├── WebPages.cc         # embedded HTML/CSS/JS pages
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

Main browser agent workspace.

The page can be used in two modes:

- direct provider mode: fill Base URL, Model, and API Key on the page, then chat through an OpenAI-compatible API such as ChatAnywhere
- local tool mode: select a local tool in the bottom composer and the page calls `/chat` through the C++ gateway

```bash
curl -i --max-time 3 http://127.0.0.1:8080/
```

Current main page behavior:

- The left rail is intentionally minimal and currently shows `Chat` and `Setup`.
- The top right account control opens a Login/Register modal backed by `/auth/login` and `/auth/register`.
- Successful login stores the session token in browser `localStorage` and restores the current user through `/auth/me`.
- The sidebar lists client-side agent cards.
- `+ New Agent` creates a temporary browser-side agent card and selects it.
- Each agent card has a small delete control; the last remaining agent is not deleted.
- Clicking an agent shows Provider settings as a child panel under that selected agent.
- Provider settings are currently shared browser-side inputs for Base URL, Model, and API Key.
- The bottom composer keeps the `Auto` / `Model` / `RAG` / `MCP` mode selector and the tool connection selector.
- Agent create/delete/select is UI-only for now; it does not yet persist to a backend `/agents` API.
- Authentication is real for demo users, but `/chat` is still allowed for guests.

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

The health response also includes demo auth metadata:

```json
{
  "auth": {
    "user_count": 0,
    "session_count": 0,
    "invite_required": false,
    "session_ttl_seconds": 86400,
    "registration_limit_per_minute": 5,
    "password_hash": "demo_only_fnv1a"
  }
}
```

### `POST /auth/register`

Create a local demo user. This endpoint is for learning and local gateway experiments, not production authentication.

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"oscar","password":"password123"}'
```

If `CPP_AI_REGISTRATION_INVITE_CODE` is set, include:

```json
{
  "username": "oscar",
  "password": "password123",
  "invite_code": "your-code"
}
```

Validation:

- username must be 3 to 32 characters
- username can contain only letters, numbers, underscores, and hyphens
- password must be 8 to 128 characters
- duplicate usernames return an error
- per-client registration attempts are limited in memory

Example response:

```json
{
  "ok": true,
  "user": {
    "username": "oscar",
    "role": "user"
  }
}
```

### `POST /auth/login`

Login with a local demo user and receive a session token.

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"oscar","password":"password123"}'
```

Example response:

```json
{
  "ok": true,
  "session_token": "random-token",
  "user": {
    "username": "oscar",
    "role": "user"
  }
}
```

Do not put the session token in a URL. Use the `Authorization` header.

### `GET /auth/me`

Return the user for a valid session token.

```bash
curl -i --max-time 3 http://127.0.0.1:8080/auth/me \
  -H 'Authorization: Bearer <session_token>'
```

Example response:

```json
{
  "ok": true,
  "user": {
    "username": "oscar",
    "role": "user"
  }
}
```

`/chat` does not require login yet. The auth boundary is intentionally separate so a later extension can protect `/chat`, `/agents/{agent_id}/chat`, per-user quota, and agent ownership without rewriting provider logic.

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
    "status": "C++ AI service gateway with tools, cache, MCP-like endpoints, and minimal RAG"
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

Current MCP-like compatibility:

- tool discovery through a stable JSON response
- tool names, descriptions, and input schemas
- tool invocation through `POST /mcp/call`

Still custom, not full MCP:

- no JSON-RPC envelope such as `jsonrpc`, `id`, `method`, and `params`
- no `initialize`, capability negotiation, or MCP session lifecycle
- no stdio/SSE transport
- no official MCP error object shape

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

Tool input can be passed as `query` or `message`:

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/mcp/call \
  -H 'Content-Type: application/json' \
  -d '{"tool":"rag_search","query":"RAG vector gateway"}'
```

### `POST /rag/ingest`

Add or replace a temporary document in the in-memory RAG store.

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/rag/ingest \
  -H 'Content-Type: application/json' \
  -d '{"id":"supabase","title":"Supabase RAG storage","content":"Supabase Postgres with pgvector can store embeddings for future cpp-ai-service RAG retrieval."}'
```

The current store is process-local. Restarting the server resets it to the default seed documents.

### `POST /chat` With RAG

Use the local retrieval tool through the normal chat gateway:

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"how does MCP work here?","tool":"rag_search"}'
```

Example response shape:

```json
{
  "ok": true,
  "tool_used": true,
  "tool": "rag_search",
  "tool_result": {
    "query": "how does MCP work here?",
    "count": 1,
    "matches": [
      {
        "id": "mcp",
        "title": "MCP-like tool endpoint",
        "content": "The current MCP-like endpoint supports GET /mcp/tools and POST /mcp/call...",
        "score": 10
      }
    ]
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
CPP_AI_REGISTRATION_INVITE_CODE=
CPP_AI_REGISTRATION_LIMIT_PER_MINUTE=5
CPP_AI_SESSION_TTL_SECONDS=86400
```

`CPP_AI_API_KEY` is the generic API key variable. The service also checks common provider variables such as `OPENAI_API_KEY`, `DEEPSEEK_API_KEY`, `OPENROUTER_API_KEY`, `GROQ_API_KEY`, and `CHATANYWHERE_API_KEY`. API keys are never printed in `/health`; only `api_key_configured` is exposed.

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
CPP_AI_PROVIDER=openrouter CPP_AI_MODEL=openrouter/free OPENROUTER_API_KEY=... ./bin/main
CPP_AI_PROVIDER=groq CPP_AI_MODEL=llama-3.1-8b-instant GROQ_API_KEY=... ./bin/main
CPP_AI_PROVIDER=ollama CPP_AI_MODEL=llama3.2 ./bin/main
CPP_AI_PROVIDER=chatanywhere CPP_AI_MODEL=gpt-3.5-turbo CHATANYWHERE_API_KEY=... ./bin/main
```

Quick free-model test with OpenRouter:

```bash
CPP_AI_PROVIDER=openrouter OPENROUTER_API_KEY=... ./bin/main
```

When `CPP_AI_MODEL` is omitted, `openrouter` defaults to `openrouter/free`.

Quick free-model test with ChatAnywhere:

```bash
CPP_AI_PROVIDER=chatanywhere CHATANYWHERE_API_KEY=... ./bin/main
```

When `CPP_AI_MODEL` is omitted, `chatanywhere` defaults to `gpt-3.5-turbo` and `CPP_AI_BASE_URL` defaults to `https://api.chatanywhere.tech/v1`.

Demo/API playground:

- `/direct` defaults to ChatAnywhere Demo.
- Select `Custom` to test your own OpenAI-compatible API by entering Base URL, Model, and API Key.
- Browser direct calls are only for demos because the key is visible to the page and browser tools.
- For real usage, start `./bin/main` with provider environment variables and use the local gateway path.

Supported network providers use an OpenAI-compatible `POST /chat/completions` API. Set `CPP_AI_BASE_URL` to point at any compatible host, including local services like LM Studio or Ollama.

If `CPP_AI_PROVIDER` is not `stub` or `ollama` and no API key is configured, the service logs a warning without exposing secrets.

## Current Limitations

- Real provider support currently targets OpenAI-compatible chat completion APIs.
- JSON parsing is intentionally minimal and currently targets simple request bodies like `{"message":"..."}`.
- `HttpCodec` now owns basic HTTP parsing/response formatting, and `WebPages` owns the embedded browser pages.
- Auth is a local demo layer. User records are stored in `data/users.jsonl`, sessions are process-local, and the password hash is marked `demo_only_fnv1a` rather than production-grade Argon2/bcrypt/scrypt/PBKDF2.
- `/chat` is still public by default; auth exists as a boundary for later protected routes and per-user features.
- The main page stores the demo session token in `localStorage`; this is convenient for local testing but not a complete browser security design.
- Main page agent cards are currently client-side UI state only. They preview the future agent workflow but do not persist across refreshes.
- Provider settings visually belong to the selected agent card, but backend provider routing is still driven by the existing request/environment configuration paths.
- Tool execution is local and manually selected by request field; there is no model-driven tool-call loop yet.
- LFU cache is connected for repeated non-tool `/chat` messages, but cache invalidation and metrics are still basic.
- MCP-like JSON endpoints exist, but full MCP JSON-RPC compatibility is not implemented yet.
- RAG storage is in-memory and uses keyword-style embeddings. It is a boundary/demo, not a production vector database.
- `/rag/ingest` stores whole documents for now; chunking is designed but not implemented.

## Roadmap

Near-term:

- Continue moving route handlers and embedded HTML out of `main.cc`
- Add a backend `/agents` API so the current UI agent cards become persistent server-side agent records
- Connect `UserStore` identities to future agents, ownership, quotas, and protected chat routes
- Add per-agent provider settings, tool allowlists, private data stores, and RAG retrieval
- Add provider-specific request options for headers, organization/project IDs, and streaming
- Add request id tracing and richer latency metrics
- Add document chunking before storage
- Add persistent vector storage, with Supabase/Postgres/pgvector as a practical option

Later:

- Add model-driven tool calling
- Move toward MCP JSON-RPC compatibility
- Replace keyword embeddings with a real embedding provider
- Add a final answer flow that automatically sends retrieved context into `AiClient`
- Use `future_extension.md` as the reference for later persistent auth, chat history, menu pages, MySQL records, speech recognition, text-to-speech, image upload/recognition, MCP config, multi-model routing, and async job queue work

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
- `data/`
- `.env`
- API keys
- local IDE files

## License

GPL-3.0. See [LICENSE](LICENSE).
