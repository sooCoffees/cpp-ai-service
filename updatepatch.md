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

## 2026-05-04

Changes:

- Added a minimal in-memory RAG implementation.
- Added `include/RagStore.h`.
- Added `src/RagStore.cc`.
- Added `EmbeddingProvider`, `KeywordEmbeddingProvider`, `VectorStore`, and `InMemoryRagStore` boundaries.
- Seeded the local RAG store with project architecture, MCP-like endpoint, and RAG planning documents.
- Added the `rag_search` tool to `ToolRegistry`.
- Updated tool execution so `/chat` passes the user message into the selected tool.
- Updated `/mcp/call` so tool input can be passed through `query` or `message`.
- Added `POST /rag/ingest` for adding or replacing temporary in-memory documents.
- Updated `project_status` to reflect the current gateway, MCP-like endpoint, and RAG capabilities.
- Updated README with RAG architecture, endpoint examples, MCP-like compatibility gaps, current limitations, and roadmap.
- Updated the learning plan statuses for 5/9 through 5/13.

Why:

- The 5/9 plan required documenting what is MCP-like today and what is still missing for full MCP JSON-RPC compatibility.
- The 5/10 plan required a gateway-oriented RAG design instead of a generic chatbot add-on.
- The 5/11 plan required a runnable local retrieval tool.
- The 5/12 plan required an embedding/vector-store boundary so keyword retrieval can later be replaced without changing `/chat`.
- The 5/13 plan required a shareable demo path in README.

Verification:

```bash
cmake -S . -B build
cmake --build build -j 4
```

Verified endpoints:

```bash
curl -i --max-time 3 http://127.0.0.1:8080/mcp/tools
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"how does MCP work here?","tool":"rag_search"}'
curl -i --max-time 3 -X POST http://127.0.0.1:8080/rag/ingest \
  -H 'Content-Type: application/json' \
  -d '{"id":"supabase","title":"Supabase RAG storage","content":"Supabase Postgres with pgvector can store embeddings for future cpp-ai-service RAG retrieval."}'
curl -i --max-time 3 -X POST http://127.0.0.1:8080/mcp/call \
  -H 'Content-Type: application/json' \
  -d '{"tool":"rag_search","query":"Supabase pgvector embeddings"}'
```

Notes:

- The RAG store is process-local and resets when the server restarts.
- The current embedding provider is keyword-based; it is a replacement boundary, not production semantic search.
- Full MCP JSON-RPC compatibility is still future work.
