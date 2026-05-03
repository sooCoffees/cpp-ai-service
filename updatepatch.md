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
