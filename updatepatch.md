# Update Patch

## 2026-05-03

Changes:

- Added OpenAI-compatible provider support.
- Added runtime provider switching for `openai`, `openai_compatible`, `deepseek`, `openrouter`, `groq`, and `ollama`.
- Added `CPP_AI_BASE_URL`, `CPP_AI_API_KEY`, and `CPP_AI_TIMEOUT_SECONDS`.
- Added `libcurl` as the real provider HTTP client.
- Extended `/health` with AI runtime fields such as `base_url` and `timeout_seconds`.
- Improved `/chat` provider failure responses with `provider` and `base_url` details.
- Added `/direct`, a browser page for entering Base URL, Model, API Key, Temperature, and Timeout.
- Added local gateway mode to `/direct`, so the page can call the local C++ `/chat` endpoint instead of calling a provider directly.
- Fixed a macOS `PollPoller` crash when browser connections closed.
- Ignored `SIGPIPE` in `main.cc` so closed client sockets do not terminate the server process.
- Added `build-asan/` to `.gitignore`.
- Simplified `AGENTS.md` into a short command/edit/upload rule file.
- Added this update log.

Why:

- The project should support multiple OpenAI-compatible providers instead of being locked to GPT/OpenAI.
- The web UI needed a direct API testing surface where provider settings can be entered in the browser.
- macOS browser testing exposed a connection close crash in the poll-based event loop path.
- The project needed a compact update log before GitHub uploads.

Verification:

```bash
cmake --build build -j 4
```

Verified endpoints:

```bash
curl -i --max-time 3 http://127.0.0.1:8080/direct
curl -i --max-time 3 http://127.0.0.1:8080/health
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"direct page smoke"}'
```

Notes:

- Direct browser calls to third-party providers may be blocked by CORS.
- If CORS blocks direct browser calls, use local gateway mode or add a dedicated C++ proxy endpoint.

## 2026-05-04

Changes:

- Added request metadata logging for gateway requests.
- Logged method, path, status, request body size, cache hit, tool usage, tool name, upstream provider latency, and total local handling latency.
- Extracted HTTP request parsing and response formatting into `HttpCodec`.
- Added `include/HttpCodec.h`.
- Added `src/HttpCodec.cc`.
- Added MCP-like tool discovery endpoint: `GET /mcp/tools`.
- Added MCP-like tool invocation endpoint: `POST /mcp/call`.
- Added basic MCP-like tool schemas through `ToolRegistry::listMcpToolsJson()`.
- Extended `AiChatResponse` with cache/tool/upstream-latency metadata so routing code can log request behavior without parsing response JSON.
- Updated README with request logging, `HttpCodec`, MCP-like endpoints, current limitations, and roadmap changes.

Why:

- The 5/6 plan required request logs that explain gateway behavior without exposing secrets.
- The 5/7 plan required reducing `main.cc` responsibilities by moving HTTP codec logic into a helper module.
- The 5/8 plan required a stable JSON shape for tool discovery and invocation before moving toward full MCP compatibility.
- README needed to match the current implementation after the 5/6-5/8 work.

Verification:

```bash
cmake -S . -B build
cmake --build build -j 4
```

Verified endpoints:

```bash
curl -i --max-time 3 http://127.0.0.1:8080/health
curl -i --max-time 3 http://127.0.0.1:8080/mcp/tools
curl -i --max-time 3 -X POST http://127.0.0.1:8080/mcp/call \
  -H 'Content-Type: application/json' \
  -d '{"tool":"server_time"}'
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"three day check"}'
```

Notes:

- The MCP-like endpoints are custom JSON endpoints, not full MCP JSON-RPC compatibility yet.
- `main.cc` still owns large HTML route handlers; only the HTTP codec layer was extracted in this patch.
