# AGENTS.md

本文件只记录 Codex/AI 助手在本仓库运行命令、改代码、提交 GitHub 时必须遵守的规则。

## 工作前

- 先看当前目录和 Git 状态：

```bash
pwd
git status --short --branch
```

- 优先使用 `rg` 搜索代码。
- 不要假设工作区是干净的。
- 不要覆盖用户已有改动。
- 不要改无关文件。
- 每次完成用户指令后，如果产生了新规则、新踩坑、新命令或新术语，要同步更新：
  - `memory.md`
  - `habit.md`
  - `terminology.md`

## 运行命令

- 构建优先用：

```bash
cmake --build build -j 4
```

- 启动服务：

```bash
./bin/main
```

- 服务默认监听：

```text
127.0.0.1:8080
```

- 如果 8080 被占用，先查进程：

```bash
lsof -nP -iTCP:8080
```

- 只有确认是本项目测试进程时，才可以 kill。
- macOS 沙箱里 bind 失败不一定是代码问题，必要时用普通 Terminal 跑。

## 验证接口

常用验证：

```bash
curl -i --max-time 3 http://127.0.0.1:8080/health
curl -i --max-time 3 http://127.0.0.1:8080/tools
curl -i --max-time 3 -X POST http://127.0.0.1:8080/chat \
  -H 'Content-Type: application/json' \
  -d '{"message":"hello"}'
```

浏览器入口：

```text
http://127.0.0.1:8080/
http://127.0.0.1:8080/direct
```

`/chat` 只能用 POST，浏览器地址栏直接访问 `/chat` 返回 405 是正确行为。

## 改代码

- 保持改动小而可运行。
- 优先沿用现有代码风格。
- 不要为了小功能引入复杂抽象。
- `main.cc` 已经偏大，新功能能拆出去就拆出去。
- macOS 和 Linux 兼容都要考虑。
- 网络底层代码尤其注意：
  - `SIGPIPE`
  - `PollPoller`
  - `EPollPoller`
  - fd 生命周期
  - connection close path

## AI Provider

- 项目支持 stub 和 OpenAI-compatible provider。
- 不要把项目写死成只支持 GPT。
- 真实 provider 调用走 OpenAI-compatible `/chat/completions` 风格。
- 不要打印 API key。
- 不要把 API key 写进仓库。
- 浏览器直连第三方 API 可能遇到 CORS；必要时走本地 gateway。

常用环境变量：

```bash
CPP_AI_PROVIDER=stub
CPP_AI_PROVIDER=openai_compatible
CPP_AI_BASE_URL=https://api.deepseek.com/v1
CPP_AI_MODEL=deepseek-chat
CPP_AI_API_KEY=...
CPP_AI_TIMEOUT_SECONDS=20
```

## 不要提交

- `build/`
- `build-asan/`
- `bin/`
- `lib/`
- `logs/`
- `.env`
- API key
- IDE 私有配置
- 大型生成文件
- 个人笔记文件：
  - `memory.md`
  - `habit.md`
  - `terminology.md`
  - `LEARNING_PLAN.md`

## GitHub 上传前

每次准备上传 GitHub 前必须先更新 `updatepatch.md`。

`updatepatch.md` 必须记录：

- 日期
- 本次改了什么
- 为什么改
- 验证方式
- 是否已经上传 GitHub

上传前检查：

```bash
git status --short --branch
git diff --stat
cmake --build build -j 4
```

只 add 应该上传的项目文件，不要顺手 add 个人笔记。

如果 push 被拒绝：

```bash
git pull --rebase origin main
git push origin main
```

## 回答用户

- 用户是为了学习，不只是要结果。
- 解释要直接、具体。
- 先讲为什么，再给命令。
- 修改后说明验证结果。
- 不要把普通聊天页面说成项目核心；项目核心是 C++ AI service gateway。
