# Registration System Risk Notes

本文档记录给 `cpp-ai-service` 增加注册系统前需要先想清楚的风险和取舍。这里的注册系统不是项目核心，项目核心仍然是 C++ AI service gateway；注册只是一层本地身份能力，用来区分用户、保护接口、以后支持配额和审计。

## 目标边界

当前 demo 版已按 MVP 实现：

- `POST /auth/register`：创建用户。
- `POST /auth/login`：登录并返回 `session_token`。
- `GET /auth/me`：根据 token 返回当前用户。
- 暂时不做邮箱验证、短信验证、OAuth、找回密码、管理员后台。
- 暂时不把 `/chat` 强制改成必须登录，除非明确决定要这样做。
- 预留 `UserProfile`、`SessionInfo`、`authenticateBearerToken()` 等接口，方便以后保护 `/chat`、`/agents/{agent_id}/chat`、配额和 agent ownership。

这个范围足够学习认证系统的核心流程，同时不会让项目被账号系统拖走。

## 并发隐患

注册系统的第一个大坑是并发。

如果两个请求同时注册同一个用户名，比如都注册 `oscar`，代码不能只做：

1. 查一下用户是否存在。
2. 发现不存在。
3. 写入用户。

因为两个线程可能同时通过第 2 步，然后都写入成功。这叫 check-then-act race。

MVP 里至少需要：

- `UserStore` 内部加 `std::mutex`。
- 查重和写入必须在同一把锁里完成。
- session token 的生成和保存也要在锁里保护。
- 文件写入要避免半写入导致数据损坏。

如果使用 `data/users.jsonl` 这种本地文件存储，推荐做法是：

- 内存里维护 `username -> user`。
- 启动时从文件加载。
- 注册成功时 append 一行。
- append 时加锁。
- 写入后检查 stream 状态。

这仍然不是强一致数据库，但足够做本地学习版。

更真实的系统应该用 SQLite/Postgres 这类数据库，并在 `username` 上加唯一约束。唯一约束比手写查重更可靠，因为最终写入动作由数据库保证。

## 机器注册隐患

只要 `/auth/register` 暴露出来，机器就可以批量注册。

本地 demo 阶段风险较低，因为服务默认监听 `127.0.0.1:8080`，外部机器通常访问不到。但如果未来部署到公网，就必须考虑防机器注册。

常见防护：

- IP 限流：同一个 IP 每分钟最多注册几次。
- 用户名限流：同一个用户名失败尝试不能无限刷。
- 密码错误限流：登录失败多次后短暂锁定。
- 注册冷却：同一个 IP 或设备短时间只能创建有限账号。
- CAPTCHA：公网产品常用，但本项目今天不建议引入。
- 邀请码：对学习项目很实用，简单、可控、依赖少。
- 邮箱验证：更完整，但会引入邮件服务。

我建议本项目第一版用最简单的策略：

- 默认只绑定 `127.0.0.1`。
- 注册接口加内存限流，例如同一个远端地址每分钟最多 5 次。当前由 `CPP_AI_REGISTRATION_LIMIT_PER_MINUTE` 配置。
- 可选加一个环境变量邀请码，例如 `CPP_AI_REGISTRATION_INVITE_CODE`。当前 demo 版已支持。

如果服务只在本机学习使用，可以先不加 CAPTCHA。

## 密码存储隐患

密码绝对不能明文保存。

但 C++ 标准库没有生产级密码哈希。`std::hash` 也不能用于密码，因为它不是密码学哈希，结果还可能随实现变化。

可选方案：

- 学习版：自定义 salt，加一个明确标注为非生产用途的 hash。
- 稍好版：使用 OpenSSL 做 SHA-256，但 SHA-256 本身仍然太快，不适合作为最终密码哈希。
- 真实版：使用 Argon2、bcrypt、scrypt、PBKDF2 这类慢哈希。

如果今天只做 MVP，我建议：

- 不存明文。
- 存 `salt` 和 `password_hash`。
- 代码里把当前 hash 方法命名清楚。当前 demo 版在 `/health` 中标记为 `demo_only_fnv1a`。
- README 里明确：这是学习版，不是生产密码存储。

如果目标是更接近真实项目，应该引入 libsodium 或 OpenSSL，并做 PBKDF2/Argon2。

## Session Token 隐患

登录后返回的 `session_token` 相当于临时钥匙。谁拿到 token，谁就能代表这个用户。

MVP 要注意：

- token 必须足够随机，不能用用户名、时间戳、递增数字拼出来。
- token 不能写进日志。
- token 不能出现在 URL query 里，应该放在 header，例如 `Authorization: Bearer <token>`。
- token 应该有过期时间。
- `/auth/me` 必须只返回必要信息，不能返回密码哈希、salt、内部状态。

本项目是 C++ gateway，后续如果加 request logging，要特别注意不要把 `Authorization` header 打出来。

## 文件存储隐患

如果第一版用本地文件，会有几个问题：

- 进程崩溃时可能写了一半。
- 多进程同时启动会互相覆盖或插入冲突。
- 删除用户、改密码会比 append-only 复杂。
- 文件权限如果太宽，其他用户可能读到密码哈希。

MVP 可以接受这些限制，但要清楚写在文档里。

建议第一版：

- 只支持单进程。
- 文件放在 `data/users.jsonl`。
- `data/` 不提交真实数据。
- 用户文件权限尽量收紧。
- 未来迁移到 SQLite。

## 路由和 JSON 隐患

当前项目的 JSON 解析是轻量字符串提取，适合简单 demo，但认证接口对输入更敏感。

风险包括：

- 特殊字符用户名。
- 超长 body。
- 缺字段或字段类型不对。
- 重复 JSON 字段。
- 转义字符导致解析结果和预期不同。

MVP 至少应该限制：

- 用户名长度，例如 3 到 32。
- 用户名字符，例如只允许字母、数字、下划线、短横线。
- 密码长度，例如 8 到 128。
- 请求 body 大小，例如不超过 8 KB。

更真实的实现应该引入 JSON parser，例如 `nlohmann/json`，而不是继续扩展手写解析。

## 和 `/chat` 的关系

有两种模式：

### 不强制登录

注册系统只是独立功能：

- `/auth/register`
- `/auth/login`
- `/auth/me`
- `/chat` 继续公开可用

优点：改动小，不影响现有 demo。

缺点：注册系统暂时没有保护核心接口。

### 强制 `/chat` 登录

调用 `/chat` 必须带：

```text
Authorization: Bearer <session_token>
```

优点：更像真实服务，可以继续做用户配额、审计、聊天历史。

缺点：会影响现有 curl、网页和 demo。所有调用方都要改。

我建议今天先不强制 `/chat` 登录。等认证接口稳定后，再单独做“保护 `/chat`”这一小步。

## 推荐今天实现顺序

已实现：

1. 新增 `UserStore`，只负责用户和 session。
2. 加 `POST /auth/register`。
3. 加 `POST /auth/login`。
4. 加 `GET /auth/me`。
5. 加基础 curl 验证路径。
6. 确认现有 `/health`、`/tools`、`/chat` 不受影响。

未做：

- 网页登录框。
- 强制 `/chat` 登录。
- 生产级密码哈希。
- SQLite/MySQL 持久化。

## 推荐第一版返回格式

注册成功：

```json
{
  "ok": true,
  "user": {
    "username": "oscar"
  }
}
```

登录成功：

```json
{
  "ok": true,
  "session_token": "random-token",
  "user": {
    "username": "oscar"
  }
}
```

当前用户：

```json
{
  "ok": true,
  "user": {
    "username": "oscar"
  }
}
```

错误：

```json
{
  "ok": false,
  "error": "username already exists"
}
```

## 我的建议

今天最稳的目标是：本地学习版注册系统，带锁、带 token、带基础限流意识，但不宣称生产可用。

不建议今天做：

- OAuth。
- 邮箱验证。
- CAPTCHA。
- 复杂权限系统。
- 多租户。
- 聊天历史。
- 管理后台。

这些都可以以后做。今天先把认证边界立起来，保证代码结构不要污染 `AiClient` 和 `ToolRegistry`。
