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

## 2026-05-04

Changes:

- Extracted embedded browser page rendering out of `src/main.cc`.
- Added `include/WebPages.h`.
- Added `src/WebPages.cc`.
- Moved the main chat page HTML/CSS/JavaScript into `renderHomePage()`.
- Moved the direct API page HTML/CSS/JavaScript into `renderDirectApiPage()`.
- Updated `main.cc` routes so `/` and `/direct` call the WebPages helper functions.
- Updated README project structure and current limitations to reflect the WebPages split.

Why:

- `main.cc` had become too large because route logic, service startup, and embedded website code lived in the same file.
- Moving page rendering into a separate module keeps `main.cc` focused on request routing and server lifecycle.

Verification:

```bash
cmake -S . -B build
cmake --build build -j 4
```

Verified endpoints:

```bash
curl -i --max-time 3 http://127.0.0.1:8080/
curl -i --max-time 3 http://127.0.0.1:8080/direct
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"web refactor check","tool":"rag_search"}'
```

Notes:

- The page behavior and routes are unchanged.
- The embedded HTML is still compiled into the C++ binary; it is only separated from `main.cc`.

## 2026-05-05

Changes:

- Added provider-specific default model selection in `AiClientConfig`.
- Set the default OpenRouter model to `openrouter/free`.
- Updated the Direct API page OpenRouter preset to `OpenRouter Free`.
- Updated README with a quick OpenRouter free-model startup command.

Why:

- The fastest practical way to make the main chat page use a real low-cost/free model is OpenRouter's OpenAI-compatible free model router.
- The user should not need to remember a model id just to test the gateway with OpenRouter.

Verification:

```bash
cmake --build build -j 4
```

Notes:

- OpenRouter still requires an API key.
- The default provider remains `stub`; real model calls start only when `CPP_AI_PROVIDER` is set.

## 2026-05-05

Changes:

- Added `chatanywhere` as an OpenAI-compatible provider shortcut.
- Added default ChatAnywhere base URL: `https://api.chatanywhere.tech/v1`.
- Added default ChatAnywhere model: `gpt-3.5-turbo`.
- Added provider-specific key lookup through `CHATANYWHERE_API_KEY`.
- Added a ChatAnywhere Free preset to the Direct API page.
- Updated README with ChatAnywhere startup commands.

Why:

- ChatAnywhere provides an OpenAI-compatible free/low-cost API proxy, so it can be integrated through the existing gateway path without a new provider adapter.
- The main chat page can use this provider by setting environment variables before starting `./bin/main`.

Verification:

```bash
cmake --build build -j 4
```

Notes:

- ChatAnywhere still requires a user-owned API key.
- The provider is treated as OpenAI-compatible and calls `/chat/completions`.

## 2026-05-05

Changes:

- Repositioned `/direct` as an API playground for demos and bring-your-own-API testing.
- Changed the Direct API page title and brand text to `API Playground`.
- Made ChatAnywhere Demo the first/default preset on `/direct`.
- Updated default Base URL to `https://api.chatanywhere.tech/v1`.
- Updated default model to `gpt-3.5-turbo`.
- Updated page copy to explain that users can select Custom and enter their own OpenAI-compatible API settings.
- Updated README with the demo/API playground positioning.

Why:

- ChatAnywhere should be treated as a demo provider, while the UI should also let users test their own OpenAI-compatible APIs.
- Browser direct calls are useful for demos, but production usage should keep keys server-side through the local gateway.

Verification:

```bash
cmake --build build -j 4
```

Notes:

- `/direct` behavior is still client-side OpenAI-compatible API testing.
- The main chat page still uses the provider selected by server environment variables.

## 2026-05-05

Changes:

- Added provider settings directly to the main `/` chat page.
- Added ChatAnywhere Demo, OpenAI, DeepSeek, OpenRouter Free, Groq, and Custom presets to the main page.
- Added Base URL, Model, and API Key inputs to the main page sidebar.
- Changed main page send behavior:
  - no local tool selected: call the configured OpenAI-compatible API directly from the browser
  - local tool selected: call the C++ gateway `/chat` endpoint
- Updated README to describe the main page's direct provider mode and local tool mode.

Why:

- The main page should be usable immediately from `http://127.0.0.1:8080/`.
- Users should not need to open `/direct` just to enter their own API configuration.

Verification:

```bash
cmake --build build -j 4
```

Notes:

- Direct provider mode exposes the API key to the browser and should be treated as demo mode.
- Local tools still go through the C++ gateway.

## 2026-05-06

Changes:

- Redesigned the main `/` page into a QClaw-style agent workspace.
- Added a compact left rail, agent sidebar, top mode chips, welcome hero, and bottom composer.
- Removed the previous five quick-action cards for a cleaner workspace.
- Changed the main page copy to English-only.
- Kept MCP/tool selection in the bottom composer instead of duplicating it in the sidebar.
- Added client-side agent creation through `+ New Agent`.
- Added client-side agent deletion with a small delete control on each agent card.
- Made the Provider settings panel appear as a child panel under the selected agent.
- Kept existing `/health`, `/tools`, direct provider mode, and local `/chat` tool mode behavior.

Why:

- The main browser page needed to look closer to the requested desktop agent workspace instead of a plain chat page.
- The agent UI should preview the future multi-agent direction before the backend `/agents` API exists.
- Provider settings are easier to understand when they belong visually to the selected agent.
- Tool selection should stay near the message composer because it affects how the next message is sent.
- The UI should stay focused on the current browser workspace while future extension planning remains local for now.

Verification:

```bash
cmake --build build -j 4
curl -i --max-time 3 http://127.0.0.1:8080/health
curl -i --max-time 3 http://127.0.0.1:8080/tools
curl -i --max-time 3 http://127.0.0.1:8080/
```

Browser verification:

- Opened `http://127.0.0.1:8080/` in the in-app browser.
- Verified that `+ New Agent` creates and selects a new agent card.
- Verified that agent deletion removes the selected generated agent.
- Verified that the Provider panel is hidden initially and appears under the selected agent card.

GitHub upload:

- Already uploaded to GitHub: Yes, commit `9333901`.

## 2026-05-06

Changes:

- Added `future_extension.md`.
- Documented future optional extensions for registration, login, chat sessions, history sync, menu pages, MySQL persistence, speech recognition, text-to-speech, image upload, image recognition, MCP config files, multi-model strategy routing, and async job queues.
- Mapped the reference handler/resource/module structure into a future `cpp-ai-service` shape while preserving the current C++ gateway focus.

Why:

- The future extension plan should be uploaded as project roadmap documentation.
- The project needs a clear separation between the current agent workspace UI and later full application features.
- The roadmap helps keep later features scoped so the project does not become only a generic chat website.

Verification:

```bash
cmake --build build -j 4
```

GitHub upload:

- Already uploaded to GitHub: No, pending this upload.
