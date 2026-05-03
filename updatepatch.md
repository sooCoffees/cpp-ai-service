# updatepatch

## 使用规则

以后每次准备上传 GitHub 前，都要先更新这个文件。

必须记录：

- 日期
- 本次改了什么
- 为什么改
- 验证方式
- 是否已经上传 GitHub

不要把个人学习/记忆文件当成项目更新记录写进来，例如：

- `memory.md`
- `habit.md`
- `terminology.md`
- `LEARNING_PLAN.md`

这些文件不应该跟项目代码更新混在一起。

## 2026-05-03

状态：已上传 GitHub

本次改动：

- 新增 OpenAI-compatible provider 支持。
- 支持通过环境变量切换 `openai`、`openai_compatible`、`deepseek`、`openrouter`、`groq`、`ollama`。
- 新增 `CPP_AI_BASE_URL`、`CPP_AI_API_KEY`、`CPP_AI_TIMEOUT_SECONDS` 配置。
- 引入 `libcurl` 作为真实 provider HTTP client。
- `/health` 增加 `base_url`、`timeout_seconds` 等 AI runtime 配置输出。
- `/chat` 在 provider 请求失败时返回更清楚的 `provider` 和 `base_url` 错误信息。
- 新增 `/direct` 页面，可以在网页里填写 Base URL、Model、API Key，并直接调用 OpenAI-compatible API。
- `/direct` 页面支持切换为 local gateway mode，通过本地 C++ `/chat` 转发。
- 修复 macOS `PollPoller` 连接关闭后崩溃的问题。
- 在 `main.cc` 增加 `SIGPIPE` 忽略，避免客户端提前断开导致服务进程退出。
- `.gitignore` 增加 `build-asan/`，避免临时 ASAN 构建目录被提交。
- 精简 `AGENTS.md`，只保留运行命令、改代码、提交 GitHub 时必须遵守的规则。
- 新增 `updatepatch.md`，作为每次 GitHub 上传前必须更新的项目变更记录。

为什么改：

- 项目不能只固定调用 GPT，需要能切不同 OpenAI-compatible provider。
- 用户希望可以直接在 Web 页面里填 API 信息并调用模型。
- macOS 下连续请求会触发 poller 连接销毁 bug，导致网页出现 `Failed to fetch`。
- `AGENTS.md` 过长会影响后续执行效率，需要把项目长说明移到 README/updatepatch，保留操作规则。
- 每次上传 GitHub 前需要有固定变更记录，避免不知道这次到底上传了什么。

验证方式：

```bash
cmake --build build -j 4
```

已验证接口：

```bash
curl -i --max-time 3 http://127.0.0.1:8080/direct
curl -i --max-time 3 http://127.0.0.1:8080/health
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"direct page smoke"}'
```

备注：

- 浏览器直连第三方 provider 可能被 CORS 拦截。
- 遇到 CORS 时应使用 `/direct` 页面里的 local gateway mode，或后续实现专门的 C++ proxy endpoint。
