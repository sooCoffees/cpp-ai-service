# Future Extension Plan

This document lists future extensions that can grow `cpp-ai-service` from a local AI gateway into a fuller AI application platform. These ideas are not required for the first agent/RAG/MCP milestone. They should be treated as optional project extensions after the core gateway, agent isolation, private data store, and chat flows are stable.

## 1. Extension Goals

The future version can support:

- user registration and login
- session and chat history
- a menu-driven web UI
- MySQL-backed persistent records
- speech recognition and text-to-speech
- image upload and image recognition
- MCP tool configuration files
- multi-model strategy routing
- message queue based async processing
- separate handler modules for each business route

High-level direction:

```text
User account
  -> agent workspace
  -> chat session
  -> message history
  -> optional voice/image input
  -> model/RAG/MCP processing
  -> persisted response
```

## 2. Proposed Project Shape

The reference structure from the diagram can be adapted into this project without copying it directly:

```text
cpp-ai-service/
  src/
    main.cc
    handlers/
      ChatSpeechHandler.cc
      ChatSessionHandler.cc
      ChatSendHandler.cc
      ChatRegisterHandler.cc
      ChatLogoutHandler.cc
      ChatLoginHandler.cc
      ChatHistoryHandler.cc
      ChatHandler.cc
      ChatEntryHandler.cc
      ChatCreateAndSendHandler.cc
      AIUploadSendHandler.cc
      AIUploadHandler.cc
      AIMenuHandler.cc
    ai/
      AIConfig.cc
      AIFactory.cc
      AIHelper.cc
      AIModelClient.cc
      AISessionIdGenerator.cc
      AISpeechProcessor.cc
      AIStrategy.cc
      AIToolRegistry.cc
      ImageRecognizer.cc
    storage/
      UserStorage.cc
      SessionStorage.cc
      MessageStorage.cc
      AgentStorage.cc
      UploadStorage.cc
    util/
      Base64.cc
      JsonUtil.cc
      PasswordHash.cc
      MimeUtil.cc
  include/
    handlers/
    ai/
    storage/
    util/
  resource/
    AI.html
    entry.html
    menu.html
    upload.html
    config.json
    NotFound.html
```

Important adjustment for this repository:

```text
Keep the current Reactor/TcpServer/HttpCodec foundation.
Add business handlers gradually.
Do not turn main.cc into a large switchboard again.
```

## 3. User Account Extension

### 3.1 Registration

Purpose:

Allow a user to create an account before using personal agents, chat history, and uploaded data.

Possible route:

```text
POST /auth/register
```

Request:

```json
{
  "email": "user@example.com",
  "password": "plain-text-from-client",
  "display_name": "Oscar"
}
```

Backend requirements:

- validate email format
- reject duplicate email
- hash password before storage
- never log raw password
- return a user id

Suggested handler:

```text
ChatRegisterHandler
```

Suggested table:

```sql
CREATE TABLE users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  email VARCHAR(255) NOT NULL UNIQUE,
  display_name VARCHAR(255),
  password_hash VARCHAR(255) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

### 3.2 Login

Purpose:

Let a registered user access personal sessions, agents, and uploaded data.

Possible route:

```text
POST /auth/login
```

Backend requirements:

- verify password hash
- create session token
- store session token safely
- return only non-secret user info

Suggested handler:

```text
ChatLoginHandler
```

### 3.3 Logout

Purpose:

Invalidate a session token.

Possible route:

```text
POST /auth/logout
```

Suggested handler:

```text
ChatLogoutHandler
```

Suggested table:

```sql
CREATE TABLE user_sessions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  session_token_hash VARCHAR(255) NOT NULL,
  expires_at TIMESTAMP NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY(user_id) REFERENCES users(id)
);
```

## 4. Chat Session and History Extension

### 4.1 Session List

Purpose:

Let the user see all previous chat sessions.

Possible route:

```text
GET /chat/sessions
```

Suggested handler:

```text
ChatSessionHandler
```

### 4.2 Create Session and Send First Message

Purpose:

Create a new chat session and process the first user message in one request.

Possible route:

```text
POST /chat/create-and-send
```

Suggested handler:

```text
ChatCreateAndSendHandler
```

### 4.3 Send Message

Purpose:

Send a message inside an existing session.

Possible route:

```text
POST /chat/send
```

Suggested handler:

```text
ChatSendHandler
```

### 4.4 History Sync

Purpose:

Allow the web UI to sync old messages after reload or login.

Possible route:

```text
GET /chat/history?session_id=...
```

Suggested handler:

```text
ChatHistoryHandler
```

Suggested tables:

```sql
CREATE TABLE chat_sessions (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  agent_id VARCHAR(128),
  title VARCHAR(255),
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY(user_id) REFERENCES users(id)
);
```

```sql
CREATE TABLE chat_messages (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  session_id BIGINT NOT NULL,
  role VARCHAR(32) NOT NULL,
  content TEXT NOT NULL,
  mode VARCHAR(32),
  provider VARCHAR(128),
  model VARCHAR(255),
  rag_used BOOLEAN NOT NULL DEFAULT FALSE,
  tool_used BOOLEAN NOT NULL DEFAULT FALSE,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY(session_id) REFERENCES chat_sessions(id)
);
```

## 5. Menu UI Extension

Purpose:

Provide a front page after login where the user can choose different AI features.

Possible pages:

```text
GET /entry
GET /menu
GET /chat
GET /upload
```

Suggested resource files:

```text
resource/entry.html
resource/menu.html
resource/AI.html
resource/upload.html
resource/NotFound.html
```

Menu items:

- Chat with model
- Agent workspace
- Upload data
- Image recognition
- Voice chat
- History
- Settings

Suggested handler:

```text
AIMenuHandler
```

## 6. MySQL Persistence Extension

SQLite is still the best first persistence step for the local gateway. MySQL becomes useful when the project wants multi-user records, account login, long-term history, and deployment closer to a real web app.

Suggested MySQL-backed data:

- users
- login sessions
- chat sessions
- chat messages
- uploaded files
- agent configs
- agent documents
- MCP/tool configs
- audit logs

Suggested storage modules:

```text
UserStorage
SessionStorage
MessageStorage
AgentStorage
UploadStorage
```

Recommendation:

```text
Start with SQLite for local agent/RAG.
Move to MySQL only when registration, login, and long-term history become real requirements.
```

## 7. Speech Recognition and Text-to-Speech Extension

### 7.1 Speech Recognition

Purpose:

Let users speak to the AI instead of typing.

Possible route:

```text
POST /speech/transcribe
```

Input:

- audio file upload
- base64 audio payload
- browser-recorded audio blob

Output:

```json
{
  "ok": true,
  "text": "What is the refund policy?"
}
```

Suggested handler:

```text
ChatSpeechHandler
```

Suggested AI module:

```text
AISpeechProcessor
```

Implementation options:

- local Whisper runtime
- OpenAI-compatible transcription endpoint
- provider-specific ASR service

### 7.2 Text-to-Speech

Purpose:

Let the assistant response be played as audio.

Possible route:

```text
POST /speech/synthesize
```

Request:

```json
{
  "text": "Refunds are available within 30 days.",
  "voice": "default"
}
```

Output:

- audio bytes
- or a URL to generated audio

Important:

- ASR means automatic speech recognition, speech to text.
- TTS means text to speech.
- These should be optional tools, not required for normal chat.

## 8. Image Upload and Recognition Extension

Purpose:

Let users upload images and ask the AI to recognize, classify, describe, or reason about the image.

Possible routes:

```text
GET /upload
POST /upload/image
POST /upload/image-and-send
```

Suggested handlers:

```text
AIUploadHandler
AIUploadSendHandler
```

Suggested AI module:

```text
ImageRecognizer
```

Implementation options:

- OpenCV for local preprocessing
- vision-capable model provider
- OCR engine for screenshots/documents
- image metadata extraction

Example response:

```json
{
  "ok": true,
  "image_id": "img_123",
  "description": "The image appears to show a code directory tree.",
  "model": "vision-model"
}
```

Storage:

```sql
CREATE TABLE uploaded_files (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT,
  session_id BIGINT,
  file_name VARCHAR(255),
  mime_type VARCHAR(128),
  storage_path TEXT NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

Security:

- validate file type
- limit file size
- do not execute uploaded files
- store uploads outside source directories

## 9. MCP Config Extension

Purpose:

Allow tools to be configured from a JSON file instead of hardcoding everything.

Suggested file:

```text
resource/config.json
```

Example:

```json
{
  "tools": [
    {
      "name": "rag_search",
      "enabled": true,
      "scope": "agent"
    },
    {
      "name": "server_time",
      "enabled": true,
      "scope": "global"
    }
  ],
  "providers": [
    {
      "name": "deepseek",
      "base_url": "https://api.deepseek.com/v1",
      "default_model": "deepseek-chat"
    }
  ]
}
```

Suggested module:

```text
AIConfig
AIToolRegistry
```

Rules:

- config can define available tools
- agent allowlist still decides which tools each agent can use
- never store API keys in committed config files

## 10. Multi-Model Strategy Extension

Purpose:

Route requests across different models based on task type, cost, speed, or fallback behavior.

Suggested module:

```text
AIStrategy
AIFactory
AIModelClient
```

Strategies:

- use cheap model for simple chat
- use stronger model for RAG synthesis
- use vision model for image input
- use ASR model for speech input
- fallback to another provider if one provider fails

Example:

```text
text chat -> ChatAnywhere/OpenRouter/DeepSeek
RAG answer -> stronger text model
image recognition -> vision model
speech transcription -> ASR provider
```

## 11. Message Queue Extension

Purpose:

Move heavy work away from the HTTP request path.

Useful for:

- speech transcription
- image recognition
- large document chunking
- embedding generation
- long-running provider calls

Suggested module:

```text
MQManager
```

First local version:

```text
in-process producer/consumer queue
```

Later production version:

```text
Redis stream, RabbitMQ, Kafka, or MySQL-backed job table
```

Suggested job table:

```sql
CREATE TABLE async_jobs (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT,
  job_type VARCHAR(64) NOT NULL,
  status VARCHAR(32) NOT NULL,
  input_json TEXT,
  output_json TEXT,
  error TEXT,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

## 12. Suggested Implementation Order

Recommended order after `extension.md` core agent plan:

```text
1. Chat history tables and APIs.
2. Session id generation and persistent chat sessions.
3. Menu and history UI.
4. Registration/login.
5. MySQL storage adapter.
6. Image upload and recognition.
7. Speech recognition.
8. Text-to-speech.
9. MCP config file.
10. Multi-model strategy routing.
11. Async job queue.
```

Reason:

- History and sessions are useful immediately.
- Login only matters once personal data and history exist.
- MySQL matters once multi-user persistence is real.
- Voice/image are high-value extensions but should not block the core gateway.
- Strategy routing and async queue are useful after multiple model/tool types exist.

## 13. Review Notes

These extensions are good portfolio features, but they should not all be built at once.

Best first extension after agent-scoped RAG:

```text
Chat history + session list
```

Best second extension:

```text
User registration/login
```

Best third extension:

```text
Image upload or speech recognition
```

Biggest risk:

```text
The project can become a generic chat website and lose the C++ AI gateway focus.
```

Keep the core identity clear:

```text
cpp-ai-service is a C++ AI service gateway with providers, tools, RAG, agents, and extensible runtime modules.
The browser pages are a control surface, not the core product.
```
