# Agent Private Data, MCP, and RAG Extension Plan

## 1. Product Goal

把 `cpp-ai-service` 从单一 AI gateway 扩展成多 agent 平台。

每个 agent 像一个独立项目：

```text
Agent = model config + system prompt + private data store + RAG retrieval + MCP tools
```

用户可以创建不同 agent，把 data 丢进某个 agent。模型本身不是在本项目里重新训练；模型仍然来自 OpenAI-compatible provider，例如 OpenAI、DeepSeek、OpenRouter、ChatAnywhere、Ollama、LM Studio。用户上传的 data 会被保存到该 agent 的独立资料库里，需要时通过 RAG 检索出来，再交给模型回答。

一句话：

```text
模型负责理解和生成答案；agent private data store 负责保存资料；RAG 负责按需取资料；MCP/tool 负责调用外部能力。
```

## 2. User Experience

目标用户流程：

```text
1. User creates an agent.
2. User configures provider/model/system prompt.
3. User uploads data into this agent.
4. User enables MCP tools for this agent.
5. User asks a question.
6. Gateway decides model-only, RAG, MCP, or auto mode.
7. Model answers with retrieved context or tool result when needed.
```

例子：

```text
support-agent
  data:
    refund_policy.txt
    shipping_policy.txt
  enabled tools:
    rag_search
    server_time

cpp-agent
  data:
    README.md
    architecture_notes.md
  enabled tools:
    rag_search
    project_status
```

`support-agent` 不能检索 `cpp-agent` 的 data。`cpp-agent` 也不能调用只给 `support-agent` 开启的 tool。

## 3. Terms

### Agent

一个独立的 AI 工作区，包含名称、说明、system prompt、provider/model 配置、私有 data、RAG store 和 MCP/tool 配置。

### Private Data Store

每个 agent 自己的资料库。用户上传的 data 不属于全局项目，而是属于某个具体 agent。

### RAG

Retrieval-Augmented Generation。先从 agent 的 data 里检索相关内容，再把这些内容作为 context 交给模型生成答案。

### MCP / Tool

agent 可以调用的外部能力或本地能力。当前项目已有 MCP-like endpoints，第一版可以继续沿用，不急着完整兼容正式 MCP JSON-RPC。

### Model-only Mode

不查 data，不调 MCP/tool，直接调用 provider/model 回答。

## 4. Recommended Technical Direction

我会按这个顺序实现：

```text
Phase 1: in-memory agent runtime
Phase 2: SQLite persistence
Phase 3: SQLite FTS5 retrieval
Phase 4: embedding/vector-store replacement boundary
Phase 5: pgvector, sqlite-vec, or Qdrant if needed
```

不要第一版直接上复杂向量库。原因：

- 这个项目当前是本地 C++ gateway。
- 第一版最重要的是 agent 隔离、API 合同和 UI 工作流。
- SQLite 本地可跑、容易 debug、容易给 mentor review。
- 向量库可以在 `VectorStore` 边界稳定后再替换。

最终推荐：

```text
Agent config: SQLite
Document metadata: SQLite
Chunks: SQLite
First retrieval: SQLite FTS5
Later semantic retrieval: sqlite-vec, pgvector, or Qdrant
MCP allowlist/config: SQLite
Model calls: existing OpenAI-compatible provider path
```

## 5. Storage Design

### 5.0 users and ownership

The current demo auth layer already has `UserStore`, `UserProfile`, `SessionInfo`, and bearer-token authentication. Future extension work should use that boundary instead of adding user parsing directly to route handlers.

Recommended future ownership fields:

```sql
CREATE TABLE users (
  id TEXT PRIMARY KEY,
  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  password_hash_algorithm TEXT NOT NULL,
  role TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE sessions (
  token_hash TEXT PRIMARY KEY,
  user_id TEXT NOT NULL,
  expires_at TEXT NOT NULL,
  created_at TEXT NOT NULL,
  FOREIGN KEY(user_id) REFERENCES users(id)
);
```

When agent persistence is added, every agent should have an owner:

```sql
ALTER TABLE agents ADD COLUMN owner_user_id TEXT NOT NULL;
```

Routes such as `/agents/{agent_id}/chat`, `/agents/{agent_id}/data`, and `/agents/{agent_id}/mcp/call` should authenticate a bearer token first, then check that the authenticated user owns or can access that agent.

### 5.1 agents

Agent 配置是结构化数据，适合 SQL。

```sql
CREATE TABLE agents (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  description TEXT,
  system_prompt TEXT,
  provider TEXT NOT NULL,
  model TEXT NOT NULL,
  base_url TEXT,
  default_mode TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);
```

字段说明：

- `id`: stable agent id，例如 `support-agent`
- `name`: UI 显示名
- `description`: agent 用途说明
- `system_prompt`: 该 agent 的行为规则
- `provider`: 上游模型服务，例如 `stub`、`openai_compatible`、`deepseek`
- `model`: 模型名
- `base_url`: OpenAI-compatible API base URL
- `default_mode`: `model_only`、`rag`、`mcp`、`auto`

### 5.2 agent_documents

保存用户上传的原始文档 metadata 和原始内容。

```sql
CREATE TABLE agent_documents (
  id TEXT PRIMARY KEY,
  agent_id TEXT NOT NULL,
  title TEXT NOT NULL,
  source_type TEXT NOT NULL,
  content TEXT NOT NULL,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY(agent_id) REFERENCES agents(id)
);
```

第一版支持：

```text
source_type = text
```

后续再支持：

```text
markdown
pdf
url
code
csv
```

### 5.3 agent_chunks

不要把整篇文档直接塞给模型。上传后需要 chunk。

```sql
CREATE TABLE agent_chunks (
  id TEXT PRIMARY KEY,
  agent_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  chunk_index INTEGER NOT NULL,
  content TEXT NOT NULL,
  token_count INTEGER,
  created_at TEXT NOT NULL,
  FOREIGN KEY(agent_id) REFERENCES agents(id),
  FOREIGN KEY(document_id) REFERENCES agent_documents(id)
);
```

第一版 chunk 策略：

```text
chunk size: about 800-1200 characters
overlap: about 100-200 characters
```

可以先按字符数切，不需要第一版就做 tokenizer。

### 5.4 agent_chunks_fts

第一版推荐 SQLite FTS5 做可运行检索。

```sql
CREATE VIRTUAL TABLE agent_chunks_fts USING fts5(
  agent_id,
  document_id,
  title,
  content
);
```

搜索时必须带 `agent_id`，避免不同 agent 的资料串起来。

```sql
SELECT document_id, title, content
FROM agent_chunks_fts
WHERE agent_id = ?
  AND agent_chunks_fts MATCH ?
LIMIT ?;
```

### 5.5 agent_tools

MCP/tool 配置是结构化配置，也适合 SQL。

```sql
CREATE TABLE agent_tools (
  agent_id TEXT NOT NULL,
  tool_name TEXT NOT NULL,
  enabled INTEGER NOT NULL DEFAULT 1,
  config_json TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  PRIMARY KEY (agent_id, tool_name),
  FOREIGN KEY(agent_id) REFERENCES agents(id)
);
```

规则：

- 未启用的 tool 不能被该 agent 调用。
- `rag_search` 调用时必须查当前 agent 的 private data store。
- 全局 `/tools` 可以继续存在，但 agent chat 只能用 agent allowlist 里的 tools。

## 6. C++ Module Breakdown

建议新增：

```text
include/AgentConfig.h
include/AgentRegistry.h
include/AgentService.h
include/AgentStorage.h
include/AgentRagStore.h

src/AgentRegistry.cc
src/AgentService.cc
src/AgentStorage.cc
src/AgentRagStore.cc
```

### 6.1 AgentConfig

```cpp
struct AgentConfig
{
    std::string id;
    std::string name;
    std::string description;
    std::string systemPrompt;
    std::string provider;
    std::string model;
    std::string baseUrl;
    std::string defaultMode;
};
```

### 6.2 AgentRegistry

负责 agent 生命周期：

```text
create agent
list agents
find agent
delete agent
load enabled tools
get agent-scoped RAG store
```

第一版可以先 in-memory：

```text
std::map<std::string, AgentRuntime>
```

第二版接 SQLite。

### 6.3 AgentService

负责业务流程，不要继续把逻辑塞进 `main.cc`。

```text
HTTP route
  -> AgentService
     -> AgentRegistry
     -> AgentRagStore
     -> ToolRegistry
     -> AiClient
     -> JSON response
```

主要方法：

```cpp
std::string createAgentJson(const std::string &body);
std::string listAgentsJson() const;
std::string ingestDataJson(const std::string &agentId, const std::string &body);
std::string ragSearchJson(const std::string &agentId, const std::string &body) const;
AiChatResponse chat(const std::string &agentId, const AgentChatRequest &request);
std::string listMcpToolsJson(const std::string &agentId) const;
std::string callMcpToolJson(const std::string &agentId, const std::string &body);
```

### 6.4 AgentStorage

负责 SQLite 读写：

```text
agents
agent_documents
agent_chunks
agent_chunks_fts
agent_tools
```

第一版如果不想马上引入 SQLite，可以先不做这个模块。但 API 和 service 设计应该预留 storage boundary。

### 6.5 AgentRagStore

实现 agent-scoped retrieval：

```text
upsert document for agent
chunk document
search chunks by agent_id
return citations
```

后续可以实现：

```text
SqliteFtsRagStore
SqliteVecRagStore
PgVectorRagStore
QdrantRagStore
```

## 7. HTTP API Design

### 7.1 Agent CRUD

```text
GET /agents
POST /agents
GET /agents/{agent_id}
DELETE /agents/{agent_id}
```

Create request:

```json
{
  "id": "support-agent",
  "name": "Support Agent",
  "description": "Answers support questions from uploaded docs",
  "system_prompt": "Answer from uploaded context when available.",
  "provider": "stub",
  "model": "stub",
  "base_url": "",
  "default_mode": "rag"
}
```

Validation:

- empty `id`: `400 Bad Request`
- empty `name`: `400 Bad Request`
- duplicate `id`: `409 Conflict`
- unknown agent: `404 Not Found`

### 7.2 Agent Data

```text
POST /agents/{agent_id}/data
GET /agents/{agent_id}/data
DELETE /agents/{agent_id}/data/{document_id}
```

Ingest request:

```json
{
  "id": "refund-policy",
  "title": "Refund Policy",
  "source_type": "text",
  "content": "Refunds are available within 30 days with a receipt."
}
```

Validation:

- unknown agent: `404 Not Found`
- empty document id/title/content: `400 Bad Request`
- document id conflict can either upsert or return `409`; choose one and document it.

Recommendation:

```text
Use upsert for first version.
```

### 7.3 Agent RAG Search

```text
POST /agents/{agent_id}/rag/search
```

Request:

```json
{
  "query": "refund window",
  "limit": 3
}
```

Response:

```json
{
  "ok": true,
  "agent_id": "support-agent",
  "results": [
    {
      "document_id": "refund-policy",
      "title": "Refund Policy",
      "score": 1.0,
      "content": "Refunds are available within 30 days with a receipt."
    }
  ]
}
```

### 7.4 Agent Chat

```text
POST /agents/{agent_id}/chat
```

Request:

```json
{
  "message": "What is the refund policy?",
  "mode": "rag"
}
```

Supported modes:

```text
model_only
rag
mcp
auto
```

Behavior:

```text
model_only:
  Call AiClient directly.

rag:
  Search current agent data.
  Build context prompt.
  Call AiClient/provider.

mcp:
  Call only tools enabled for this agent.

auto:
  First version can default to rag when data exists, otherwise model_only.
```

RAG response should include citations:

```json
{
  "ok": true,
  "agent_id": "support-agent",
  "mode": "rag",
  "rag_used": true,
  "citations": [
    {
      "document_id": "refund-policy",
      "title": "Refund Policy"
    }
  ],
  "reply": "Refunds are available within 30 days with a receipt."
}
```

### 7.5 Agent MCP

```text
GET /agents/{agent_id}/mcp/tools
POST /agents/{agent_id}/mcp/tools
POST /agents/{agent_id}/mcp/call
```

Enable tools request:

```json
{
  "enabled_tools": ["rag_search", "server_time", "project_status"]
}
```

Call tool request:

```json
{
  "tool": "rag_search",
  "query": "refund policy"
}
```

Rules:

- Tool must exist in global `ToolRegistry`.
- Tool must be enabled for the current agent.
- Tool must not access another agent's data.

## 8. Prompt Construction

RAG mode should use stable prompt structure.

```text
System:
You are {agent.name}.
{agent.system_prompt}

Use the provided context when relevant.
If the context is insufficient, say that you do not have enough information from the uploaded data.

Context:
[doc: refund-policy | title: Refund Policy]
Refunds are available within 30 days with a receipt.

User:
What is the refund policy?
```

Do not silently pretend the uploaded data says something it does not say.

## 9. UI Breakdown

Main page can become a 3-panel app:

```text
Left sidebar:
  Agent list
  New Agent button

Center:
  Chat messages
  Mode selector: Model / RAG / MCP / Auto
  Message input

Right panel:
  Agent settings
  Data upload
  MCP tools button/list
```

First UI version:

- Create agent.
- Select active agent.
- Add text data through textarea.
- See data list.
- Choose chat mode.
- See MCP tools.
- Enable/disable tools.

Do not start with PDF upload, login, or vector database UI.

## 10. Internship-Style Ticket Breakdown

### Ticket 1: AgentConfig and AgentRegistry

Scope:

- Add `AgentConfig`.
- Add in-memory `AgentRegistry`.
- Support create/list/find/delete.
- No SQLite yet.

Acceptance:

```bash
cmake --build build -j 4
```

### Ticket 2: `/agents` HTTP Routes

Scope:

- `GET /agents`
- `POST /agents`
- `GET /agents/{id}`
- `DELETE /agents/{id}`

Acceptance:

```bash
curl -i --max-time 3 http://127.0.0.1:8080/agents
curl -i --max-time 3 -X POST http://127.0.0.1:8080/agents \
  -H 'Content-Type: application/json' \
  -d '{"id":"support-agent","name":"Support Agent","provider":"stub","model":"stub","default_mode":"rag"}'
```

### Ticket 3: Agent-Scoped Data Ingest

Scope:

- `POST /agents/{id}/data`
- `GET /agents/{id}/data`
- Each agent owns its own `InMemoryRagStore` for first version.

Acceptance:

- Data uploaded to Agent A does not appear in Agent B.
- Unknown agent returns `404`.
- Empty content returns `400`.

### Ticket 4: Agent-Scoped RAG Search

Scope:

- `POST /agents/{id}/rag/search`
- Search only current agent data.
- Return citations.

Acceptance:

```bash
curl -i --max-time 3 -X POST http://127.0.0.1:8080/agents/support-agent/rag/search \
  -H 'Content-Type: application/json' \
  -d '{"query":"refund window","limit":3}'
```

### Ticket 5: Agent Chat Modes

Scope:

- `POST /agents/{id}/chat`
- Support `model_only`.
- Support `rag`.
- Return `mode`, `rag_used`, and citations.

Acceptance:

- `model_only` does not search data.
- `rag` searches only current agent data.
- Unknown mode returns `400`.

### Ticket 6: Agent MCP Tool Isolation

Scope:

- `GET /agents/{id}/mcp/tools`
- `POST /agents/{id}/mcp/tools`
- `POST /agents/{id}/mcp/call`
- Enforce tool allowlist.

Acceptance:

- Disabled tool cannot be called.
- `rag_search` only searches current agent data.

### Ticket 7: SQLite Storage

Scope:

- Add SQLite dependency.
- Implement `AgentStorage`.
- Persist agents, documents, chunks, and tool allowlist.

Acceptance:

- Restarting `./bin/main` does not lose agents or documents.
- User-uploaded data is not committed to GitHub.

### Ticket 8: SQLite FTS5 RAG

Scope:

- Add `agent_chunks_fts`.
- Search chunks through FTS5.
- Keep `VectorStore` boundary.

Acceptance:

- API stays the same.
- Search quality improves over exact keyword matching.

### Ticket 9: Web UI Agent Panels

Scope:

- Agent sidebar.
- Agent creation form.
- Data upload panel.
- MCP tools panel.
- Chat mode selector.

Acceptance:

- User can create agent, upload text data, select mode, and chat from browser.

### Ticket 10: Future Semantic Vector Store

Scope:

- Add real embedding provider.
- Add one vector backend:
  - `sqlite-vec` for local semantic search
  - `pgvector` for Supabase/Postgres
  - `Qdrant` for larger vector search

Acceptance:

- Existing HTTP API does not change.
- `AgentRagStore` implementation can be swapped.

## 11. PR Sequence

Recommended PR order:

```text
PR 1: Agent registry foundation
PR 2: Agent CRUD HTTP APIs
PR 3: Agent-scoped data and RAG search
PR 4: Agent chat modes
PR 5: Agent MCP tool isolation
PR 6: SQLite persistence
PR 7: SQLite FTS5 retrieval
PR 8: Web UI agent/data/MCP panels
PR 9: semantic vector-store backend
```

Each PR should:

- build successfully
- include curl verification
- keep unrelated files untouched
- avoid committing user data, API keys, `build/`, `bin/`, or personal notes

## 12. What Not To Build First

Do not include these in version 1:

- real model fine-tuning
- login and multi-user permissions
- PDF parser
- full MCP JSON-RPC compatibility
- Qdrant deployment
- agent marketplace
- complex auto-planning agent loop
- browser direct storage of secret API keys

These are later extensions after agent isolation and RAG flow are stable.

## 13. First Implementation Recommendation

Best first implementation:

```text
1. Add in-memory AgentRegistry.
2. Give each agent its own InMemoryRagStore.
3. Add /agents APIs.
4. Add /agents/{id}/data.
5. Add /agents/{id}/rag/search.
6. Add /agents/{id}/chat with model_only and rag.
7. Add per-agent tool allowlist.
8. Add SQLite only after API behavior is reviewed.
```

This keeps the project small, reviewable, and runnable while still moving toward the real product shape.
