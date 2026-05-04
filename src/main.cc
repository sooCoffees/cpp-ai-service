#include <string>

#include <TcpServer.h>
#include <Logger.h>
#include <sys/stat.h>
#include <libgen.h>
#include <signal.h>
#include <chrono>
#include <sstream>
#include "AiClient.h"
#include "AsyncLogging.h"
#include "HttpCodec.h"
#include "ToolRegistry.h"
#include "memoryPool.h"
// 日志文件滚动大小为1MB (1*1024*1024 bytes)
static const off_t kRollSize = 1*1024*1024;

namespace
{
struct RouteResult
{
    std::string response;
    std::string status;
    bool cacheHit;
    bool toolUsed;
    std::string toolName;
    long upstreamLatencyMs;
};

RouteResult makeRouteResult(const std::string &response, const std::string &status)
{
    RouteResult result;
    result.response = response;
    result.status = status;
    result.cacheHit = false;
    result.toolUsed = false;
    result.upstreamLatencyMs = 0;
    return result;
}

RouteResult handleChatRequest(const HttpRequest &request, AiClient &aiClient)
{
    if (request.method != "POST")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/chat expects POST")),
                               "405 Method Not Allowed");
    }

    std::string message;
    if (!extractJsonStringField(request.body, "message", &message) || message.empty())
    {
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError("request body must contain a non-empty string field named message")),
                               "400 Bad Request");
    }

    std::string tool;
    extractJsonStringField(request.body, "tool", &tool);

    AiChatRequest chatRequest;
    chatRequest.message = message;
    chatRequest.tool = tool;

    const AiChatResponse chatResponse = aiClient.chat(chatRequest);
    RouteResult result = makeRouteResult(jsonResponse(chatResponse.status, chatResponse.body), chatResponse.status);
    result.cacheHit = chatResponse.cacheHit;
    result.toolUsed = chatResponse.toolUsed;
    result.toolName = chatResponse.toolName;
    result.upstreamLatencyMs = chatResponse.upstreamLatencyMs;
    return result;
}

std::string handleHomeRequest()
{
    const std::string body = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>cpp-ai-service</title>
<style>
:root {
  color-scheme: dark;
  --bg: #111312;
  --panel: #181b1a;
  --panel-2: #202422;
  --border: #303633;
  --text: #f3f5f2;
  --muted: #9aa39d;
  --accent: #19c37d;
  --danger: #ff6b6b;
  --shadow: 0 18px 60px rgba(0, 0, 0, 0.28);
}
* { box-sizing: border-box; }
html, body { height: 100%; }
body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}
button, textarea, select { font: inherit; }
.app {
  min-height: 100vh;
  display: grid;
  grid-template-columns: 280px minmax(0, 1fr);
}
.sidebar {
  border-right: 1px solid var(--border);
  background: #151716;
  padding: 18px 14px;
  display: flex;
  flex-direction: column;
  gap: 18px;
}
.brand {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 4px 6px;
  font-weight: 700;
}
.logo {
  width: 34px;
  height: 34px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  background: var(--accent);
  color: #07110c;
  font-weight: 900;
}
.new-chat, .send {
  border: 0;
  cursor: pointer;
  border-radius: 8px;
  background: var(--text);
  color: #101211;
  font-weight: 700;
}
.new-chat {
  width: 100%;
  min-height: 42px;
}
.side-block {
  border-top: 1px solid var(--border);
  padding-top: 16px;
}
.side-label {
  margin: 0 0 8px;
  color: var(--muted);
  font-size: 12px;
  text-transform: uppercase;
  letter-spacing: 0;
}
select {
  width: 100%;
  min-height: 40px;
  color: var(--text);
  background: var(--panel-2);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 0 10px;
}
.hint {
  margin: 8px 0 0;
  color: var(--muted);
  font-size: 13px;
  line-height: 1.45;
}
.main {
  min-width: 0;
  display: grid;
  grid-template-rows: auto minmax(0, 1fr) auto;
}
.topbar {
  min-height: 60px;
  border-bottom: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 0 22px;
}
.title {
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.title strong { font-size: 15px; }
.status {
  color: var(--muted);
  font-size: 13px;
}
.status.ok { color: var(--accent); }
.status.error { color: var(--danger); }
.messages {
  overflow-y: auto;
  padding: 28px 18px;
}
.message {
  width: min(860px, 100%);
  margin: 0 auto 18px;
  display: grid;
  grid-template-columns: 36px minmax(0, 1fr);
  gap: 14px;
}
.avatar {
  width: 36px;
  height: 36px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  background: var(--panel-2);
  color: var(--muted);
  font-size: 13px;
  font-weight: 800;
}
.assistant .avatar {
  background: rgba(25, 195, 125, 0.18);
  color: var(--accent);
}
.bubble {
  min-width: 0;
  padding: 10px 0;
  line-height: 1.65;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}
.assistant .bubble {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 14px 16px;
  box-shadow: var(--shadow);
}
.composer {
  border-top: 1px solid var(--border);
  padding: 16px 18px 22px;
  background: rgba(17, 19, 18, 0.92);
}
.composer-inner {
  width: min(860px, 100%);
  margin: 0 auto;
  display: grid;
  grid-template-columns: minmax(0, 1fr) 48px;
  gap: 10px;
  align-items: end;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 10px;
}
textarea {
  width: 100%;
  min-height: 48px;
  max-height: 180px;
  resize: none;
  border: 0;
  outline: 0;
  color: var(--text);
  background: transparent;
  line-height: 1.5;
  padding: 11px 8px;
}
.send {
  width: 48px;
  height: 48px;
  display: grid;
  place-items: center;
  font-size: 20px;
}
.send:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}
@media (max-width: 760px) {
  .app { grid-template-columns: 1fr; }
  .sidebar {
    border-right: 0;
    border-bottom: 1px solid var(--border);
    padding: 12px;
  }
  .side-block { display: none; }
  .topbar { padding: 0 14px; }
  .messages { padding: 20px 14px; }
  .message {
    grid-template-columns: 32px minmax(0, 1fr);
    gap: 10px;
  }
  .avatar {
    width: 32px;
    height: 32px;
  }
}
</style>
</head>
<body>
<div class="app">
  <aside class="sidebar">
    <div class="brand"><div class="logo">AI</div><span>cpp-ai-service</span></div>
    <button class="new-chat" id="newChat">New chat</button>
    <div class="side-block">
      <p class="side-label">Tool</p>
      <select id="toolSelect">
        <option value="">No tool</option>
      </select>
      <p class="hint">Tools are served by the local C++ ToolRegistry through /tools.</p>
    </div>
    <div class="side-block">
      <p class="side-label">Gateway</p>
      <p class="hint">This UI posts to /chat. The current backend still returns a stub AI reply unless a local tool is selected.</p>
    </div>
  </aside>
  <main class="main">
    <header class="topbar">
      <div class="title">
        <strong>Chat</strong>
        <span class="status" id="status">Checking service...</span>
      </div>
    </header>
    <section class="messages" id="messages"></section>
    <form class="composer" id="chatForm">
      <div class="composer-inner">
        <textarea id="messageInput" rows="1" placeholder="Message cpp-ai-service"></textarea>
        <button class="send" id="sendButton" type="submit" title="Send">↑</button>
      </div>
    </form>
  </main>
</div>
<script>
const messages = document.getElementById('messages');
const form = document.getElementById('chatForm');
const input = document.getElementById('messageInput');
const sendButton = document.getElementById('sendButton');
const statusEl = document.getElementById('status');
const toolSelect = document.getElementById('toolSelect');
const newChat = document.getElementById('newChat');

function setStatus(text, state) {
  statusEl.textContent = text;
  statusEl.className = 'status' + (state ? ' ' + state : '');
}

function addMessage(role, text) {
  const item = document.createElement('article');
  item.className = 'message ' + role;
  const avatar = document.createElement('div');
  avatar.className = 'avatar';
  avatar.textContent = role === 'user' ? 'You' : 'AI';
  const bubble = document.createElement('div');
  bubble.className = 'bubble';
  bubble.textContent = text;
  item.appendChild(avatar);
  item.appendChild(bubble);
  messages.appendChild(item);
  messages.scrollTop = messages.scrollHeight;
  return bubble;
}

function resetChat() {
  messages.innerHTML = '';
  addMessage('assistant', 'Hi, I am cpp-ai-service. Send a message to test the C++ /chat gateway, or select a local tool from the sidebar.');
  input.focus();
}

function resizeInput() {
  input.style.height = 'auto';
  input.style.height = Math.min(input.scrollHeight, 180) + 'px';
}

async function loadHealth() {
  try {
    const res = await fetch('/health');
    if (!res.ok) throw new Error('HTTP ' + res.status);
    setStatus('Service online', 'ok');
  } catch (err) {
    setStatus('Service unavailable', 'error');
  }
}

async function loadTools() {
  try {
    const res = await fetch('/tools');
    const data = await res.json();
    if (!data.ok || !Array.isArray(data.tools)) return;
    data.tools.forEach((tool) => {
      const option = document.createElement('option');
      option.value = tool.name;
      option.textContent = tool.name;
      option.title = tool.description || '';
      toolSelect.appendChild(option);
    });
  } catch (err) {
    // Tools are optional for the UI.
  }
}

async function sendMessage(text) {
  const payload = { message: text };
  if (toolSelect.value) payload.tool = toolSelect.value;

  const res = await fetch('/chat', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  const data = await res.json();
  if (!res.ok || data.ok === false) {
    throw new Error(data.error || ('HTTP ' + res.status));
  }

  let reply = data.reply || '';
  if (data.tool_used && data.tool_result) {
    reply += '\n\nTool result:\n' + JSON.stringify(data.tool_result, null, 2);
  }
  return reply || '(empty response)';
}

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  const text = input.value.trim();
  if (!text) return;

  addMessage('user', text);
  input.value = '';
  resizeInput();
  input.disabled = true;
  sendButton.disabled = true;
  setStatus('Thinking...');
  const pending = addMessage('assistant', '...');

  try {
    pending.textContent = await sendMessage(text);
    setStatus('Service online', 'ok');
  } catch (err) {
    pending.textContent = 'Request failed: ' + err.message;
    setStatus('Request failed', 'error');
  } finally {
    input.disabled = false;
    sendButton.disabled = false;
    input.focus();
  }
});

input.addEventListener('input', resizeInput);
input.addEventListener('keydown', (event) => {
  if (event.key === 'Enter' && !event.shiftKey) {
    event.preventDefault();
    form.requestSubmit();
  }
});
newChat.addEventListener('click', resetChat);

resetChat();
resizeInput();
loadHealth();
loadTools();
</script>
</body>
</html>)HTML";
    return httpResponse("200 OK", "text/html; charset=utf-8", body);
}

std::string handleDirectApiRequest()
{
    const std::string body = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Direct API Chat</title>
<style>
:root {
  color-scheme: dark;
  --bg: #101211;
  --panel: #181b1a;
  --field: #202522;
  --border: #303633;
  --text: #f4f6f3;
  --muted: #9ca49f;
  --accent: #19c37d;
  --danger: #ff6b6b;
}
* { box-sizing: border-box; }
html, body { height: 100%; }
body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}
button, input, textarea, select { font: inherit; }
.app {
  min-height: 100vh;
  display: grid;
  grid-template-columns: 340px minmax(0, 1fr);
}
.settings {
  border-right: 1px solid var(--border);
  background: #151716;
  padding: 18px;
  display: flex;
  flex-direction: column;
  gap: 14px;
}
.brand {
  display: flex;
  align-items: center;
  gap: 10px;
  font-weight: 800;
  margin-bottom: 8px;
}
.logo {
  width: 34px;
  height: 34px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  background: var(--accent);
  color: #07110c;
}
label {
  display: grid;
  gap: 6px;
  color: var(--muted);
  font-size: 12px;
  text-transform: uppercase;
  letter-spacing: 0;
}
input, textarea, select {
  width: 100%;
  color: var(--text);
  background: var(--field);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 11px 12px;
  outline: 0;
}
input:focus, textarea:focus, select:focus {
  border-color: rgba(25, 195, 125, 0.75);
}
.hint {
  margin: 0;
  color: var(--muted);
  font-size: 13px;
  line-height: 1.45;
}
.main {
  min-width: 0;
  display: grid;
  grid-template-rows: auto minmax(0, 1fr) auto;
}
.topbar {
  min-height: 62px;
  border-bottom: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 0 24px;
}
.title {
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.title strong { font-size: 15px; }
.status {
  color: var(--muted);
  font-size: 13px;
}
.status.ok { color: var(--accent); }
.status.error { color: var(--danger); }
.messages {
  overflow-y: auto;
  padding: 28px 18px;
}
.message {
  width: min(920px, 100%);
  margin: 0 auto 18px;
  display: grid;
  grid-template-columns: 40px minmax(0, 1fr);
  gap: 14px;
}
.avatar {
  width: 38px;
  height: 38px;
  border-radius: 8px;
  display: grid;
  place-items: center;
  background: var(--field);
  color: var(--muted);
  font-size: 12px;
  font-weight: 800;
}
.assistant .avatar {
  background: rgba(25, 195, 125, 0.18);
  color: var(--accent);
}
.bubble {
  min-width: 0;
  padding: 10px 0;
  line-height: 1.6;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}
.assistant .bubble {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 14px 16px;
}
.composer {
  border-top: 1px solid var(--border);
  padding: 16px 18px 22px;
  background: rgba(16, 18, 17, 0.94);
}
.composer-inner {
  width: min(920px, 100%);
  margin: 0 auto;
  display: grid;
  grid-template-columns: minmax(0, 1fr) 52px;
  gap: 10px;
  align-items: end;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 10px;
}
#messageInput {
  min-height: 52px;
  max-height: 180px;
  resize: none;
  border: 0;
  background: transparent;
}
.send, .secondary {
  border: 0;
  cursor: pointer;
  border-radius: 8px;
  font-weight: 800;
}
.send {
  height: 52px;
  background: var(--text);
  color: #101211;
  font-size: 20px;
}
.secondary {
  min-height: 40px;
  background: var(--field);
  color: var(--text);
  border: 1px solid var(--border);
}
button:disabled {
  cursor: not-allowed;
  opacity: 0.5;
}
.row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
}
@media (max-width: 820px) {
  .app { grid-template-columns: 1fr; }
  .settings { border-right: 0; border-bottom: 1px solid var(--border); }
  .row { grid-template-columns: 1fr; }
}
</style>
</head>
<body>
<div class="app">
  <aside class="settings">
    <div class="brand"><div class="logo">API</div><span>Direct API Chat</span></div>
    <p class="hint">This page can call an OpenAI-compatible API directly from the browser. Some providers block browser calls with CORS; in that case use the local gateway button.</p>

    <label>Preset
      <select id="preset">
        <option value="https://api.openai.com/v1|gpt-4.1-mini">OpenAI</option>
        <option value="https://api.deepseek.com/v1|deepseek-chat">DeepSeek</option>
        <option value="https://openrouter.ai/api/v1|openai/gpt-4.1-mini">OpenRouter</option>
        <option value="https://api.groq.com/openai/v1|llama-3.1-8b-instant">Groq</option>
        <option value="http://127.0.0.1:11434/v1|llama3.2">Ollama local</option>
        <option value="custom|">Custom</option>
      </select>
    </label>

    <label>Base URL
      <input id="baseUrl" spellcheck="false" value="https://api.openai.com/v1">
    </label>

    <label>Model
      <input id="model" spellcheck="false" value="gpt-4.1-mini">
    </label>

    <label>API Key
      <input id="apiKey" type="password" spellcheck="false" placeholder="sk-...">
    </label>

    <div class="row">
      <label>Temperature
        <input id="temperature" type="number" min="0" max="2" step="0.1" value="0.2">
      </label>
      <label>Timeout ms
        <input id="timeoutMs" type="number" min="1000" step="1000" value="30000">
      </label>
    </div>

    <button class="secondary" id="saveSettings" type="button">Save settings</button>
    <button class="secondary" id="gatewayMode" type="button">Send through local gateway</button>
    <button class="secondary" id="newChat" type="button">New chat</button>
    <p class="hint">Direct browser mode exposes your key to this page and browser devtools. Do not use keys you cannot rotate.</p>
  </aside>

  <main class="main">
    <header class="topbar">
      <div class="title">
        <strong>Chat</strong>
        <span class="status" id="status">Direct browser mode</span>
      </div>
    </header>
    <section class="messages" id="messages"></section>
    <form class="composer" id="chatForm">
      <div class="composer-inner">
        <textarea id="messageInput" rows="1" placeholder="Message direct API"></textarea>
        <button class="send" id="sendButton" type="submit" title="Send">↑</button>
      </div>
    </form>
  </main>
</div>

<script>
const messages = document.getElementById('messages');
const form = document.getElementById('chatForm');
const input = document.getElementById('messageInput');
const sendButton = document.getElementById('sendButton');
const statusEl = document.getElementById('status');
const preset = document.getElementById('preset');
const baseUrl = document.getElementById('baseUrl');
const model = document.getElementById('model');
const apiKey = document.getElementById('apiKey');
const temperature = document.getElementById('temperature');
const timeoutMs = document.getElementById('timeoutMs');
const saveSettings = document.getElementById('saveSettings');
const gatewayMode = document.getElementById('gatewayMode');
const newChat = document.getElementById('newChat');
let useGateway = false;
const history = [];

function setStatus(text, state) {
  statusEl.textContent = text;
  statusEl.className = 'status' + (state ? ' ' + state : '');
}

function addMessage(role, text) {
  const item = document.createElement('article');
  item.className = 'message ' + role;
  const avatar = document.createElement('div');
  avatar.className = 'avatar';
  avatar.textContent = role === 'user' ? 'You' : 'AI';
  const bubble = document.createElement('div');
  bubble.className = 'bubble';
  bubble.textContent = text;
  item.appendChild(avatar);
  item.appendChild(bubble);
  messages.appendChild(item);
  messages.scrollTop = messages.scrollHeight;
  return bubble;
}

function saveConfig() {
  localStorage.setItem('directApi.baseUrl', baseUrl.value.trim());
  localStorage.setItem('directApi.model', model.value.trim());
  localStorage.setItem('directApi.temperature', temperature.value);
  localStorage.setItem('directApi.timeoutMs', timeoutMs.value);
}

function loadConfig() {
  baseUrl.value = localStorage.getItem('directApi.baseUrl') || baseUrl.value;
  model.value = localStorage.getItem('directApi.model') || model.value;
  temperature.value = localStorage.getItem('directApi.temperature') || temperature.value;
  timeoutMs.value = localStorage.getItem('directApi.timeoutMs') || timeoutMs.value;
}

function resetChat() {
  messages.innerHTML = '';
  history.length = 0;
  addMessage('assistant', 'Fill Base URL, Model, and API Key on the left. This page can call /chat/completions directly from the browser.');
  input.focus();
}

function resizeInput() {
  input.style.height = 'auto';
  input.style.height = Math.min(input.scrollHeight, 180) + 'px';
}

function providerUrl() {
  return baseUrl.value.trim().replace(/\/+$/, '') + '/chat/completions';
}

async function fetchWithTimeout(url, options, timeout) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeout);
  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } finally {
    clearTimeout(timer);
  }
}

async function sendDirect(text) {
  const key = apiKey.value.trim();
  const headers = { 'Content-Type': 'application/json' };
  if (key) headers.Authorization = 'Bearer ' + key;

  const messagesForApi = history.concat([{ role: 'user', content: text }]);
  const res = await fetchWithTimeout(providerUrl(), {
    method: 'POST',
    headers,
    body: JSON.stringify({
      model: model.value.trim(),
      messages: messagesForApi,
      temperature: Number(temperature.value || 0.2)
    })
  }, Number(timeoutMs.value || 30000));

  let data;
  const raw = await res.text();
  try {
    data = JSON.parse(raw);
  } catch (err) {
    throw new Error('Provider returned non-JSON response: ' + raw.slice(0, 180));
  }
  if (!res.ok) {
    throw new Error((data.error && (data.error.message || data.error)) || ('HTTP ' + res.status));
  }
  const reply = data.choices && data.choices[0] && data.choices[0].message && data.choices[0].message.content;
  if (!reply) throw new Error('No assistant message in provider response');
  history.push({ role: 'user', content: text });
  history.push({ role: 'assistant', content: reply });
  return reply;
}

async function sendGateway(text) {
  const res = await fetch('/chat', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ message: text })
  });
  const data = await res.json();
  if (!res.ok || data.ok === false) {
    throw new Error(data.error || ('HTTP ' + res.status));
  }
  return data.reply || '(empty response)';
}

preset.addEventListener('change', () => {
  const value = preset.value;
  if (value === 'custom|') return;
  const parts = value.split('|');
  baseUrl.value = parts[0] || baseUrl.value;
  model.value = parts[1] || model.value;
  saveConfig();
});

saveSettings.addEventListener('click', () => {
  saveConfig();
  setStatus('Settings saved', 'ok');
});

gatewayMode.addEventListener('click', () => {
  useGateway = !useGateway;
  gatewayMode.textContent = useGateway ? 'Send direct from browser' : 'Send through local gateway';
  setStatus(useGateway ? 'Local gateway mode' : 'Direct browser mode', 'ok');
});

newChat.addEventListener('click', resetChat);

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  const text = input.value.trim();
  if (!text) return;

  saveConfig();
  addMessage('user', text);
  input.value = '';
  resizeInput();
  input.disabled = true;
  sendButton.disabled = true;
  setStatus(useGateway ? 'Calling local gateway...' : 'Calling provider...');
  const pending = addMessage('assistant', '...');

  try {
    pending.textContent = useGateway ? await sendGateway(text) : await sendDirect(text);
    setStatus(useGateway ? 'Local gateway mode' : 'Direct browser mode', 'ok');
  } catch (err) {
    pending.textContent = 'Request failed: ' + err.message + '\n\nIf this is a browser CORS error, switch to local gateway mode or use a provider that allows browser requests.';
    setStatus('Request failed', 'error');
  } finally {
    input.disabled = false;
    sendButton.disabled = false;
    input.focus();
  }
});

input.addEventListener('input', resizeInput);
input.addEventListener('keydown', (event) => {
  if (event.key === 'Enter' && !event.shiftKey) {
    event.preventDefault();
    form.requestSubmit();
  }
});

loadConfig();
resetChat();
resizeInput();
</script>
</body>
</html>)HTML";
    return httpResponse("200 OK", "text/html; charset=utf-8", body);
}

RouteResult handleToolsRequest(const HttpRequest &request, const ToolRegistry &tools)
{
    if (request.method != "GET")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/tools expects GET")),
                               "405 Method Not Allowed");
    }

    return makeRouteResult(jsonResponse("200 OK", tools.listToolsJson()), "200 OK");
}

RouteResult handleMcpToolsRequest(const HttpRequest &request, const ToolRegistry &tools)
{
    if (request.method != "GET")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/mcp/tools expects GET")),
                               "405 Method Not Allowed");
    }

    return makeRouteResult(jsonResponse("200 OK", tools.listMcpToolsJson()), "200 OK");
}

RouteResult handleMcpCallRequest(const HttpRequest &request, const ToolRegistry &tools)
{
    if (request.method != "POST")
    {
        return makeRouteResult(jsonResponse("405 Method Not Allowed", jsonError("/mcp/call expects POST")),
                               "405 Method Not Allowed");
    }

    std::string tool;
    if (!extractJsonStringField(request.body, "tool", &tool) || tool.empty())
    {
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError("request body must contain a non-empty string field named tool")),
                               "400 Bad Request");
    }

    const ToolResult toolResult = tools.execute(tool);
    if (!toolResult.ok)
    {
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError(toolResult.error)), "400 Bad Request");
    }

    std::ostringstream body;
    body << "{"
         << "\"ok\":true,"
         << "\"protocol\":\"mcp-like\","
         << "\"tool\":\"" << jsonEscape(tool) << "\","
         << "\"result\":" << toolResult.json
         << "}";

    RouteResult result = makeRouteResult(jsonResponse("200 OK", body.str()), "200 OK");
    result.toolUsed = true;
    result.toolName = tool;
    return result;
}

RouteResult handleHealthRequest(const AiClient &aiClient)
{
    return makeRouteResult(jsonResponse("200 OK", "{\"ok\":true,\"ai\":" + aiClient.configJson() + "}"), "200 OK");
}

RouteResult handleRequest(const HttpRequest &request, AiClient &aiClient, const ToolRegistry &tools)
{
    if (request.path == "/chat")
    {
        return handleChatRequest(request, aiClient);
    }
    if (request.path == "/tools")
    {
        return handleToolsRequest(request, tools);
    }
    if (request.path == "/health")
    {
        return handleHealthRequest(aiClient);
    }
    if (request.path == "/mcp/tools")
    {
        return handleMcpToolsRequest(request, tools);
    }
    if (request.path == "/mcp/call")
    {
        return handleMcpCallRequest(request, tools);
    }
    if (request.path == "/direct")
    {
        return makeRouteResult(handleDirectApiRequest(), "200 OK");
    }
    if (request.path == "/")
    {
        return makeRouteResult(handleHomeRequest(), "200 OK");
    }

    return makeRouteResult(jsonResponse("404 Not Found", jsonError("route not found")), "404 Not Found");
}

RouteResult handleRawRequest(const std::string &raw, AiClient &aiClient, const ToolRegistry &tools, HttpRequest *parsedRequest)
{
    HttpRequest request;
    if (!parseHttpRequest(raw, &request))
    {
        if (parsedRequest != nullptr)
        {
            *parsedRequest = request;
        }
        return makeRouteResult(jsonResponse("400 Bad Request", jsonError("malformed HTTP request")),
                               "400 Bad Request");
    }

    if (parsedRequest != nullptr)
    {
        *parsedRequest = request;
    }

    return handleRequest(request, aiClient, tools);
}

}

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
        , tools_(ToolRegistry::createDefault())
        , aiConfig_(AiClientConfig::fromEnvironment())
        , aiClient_(&tools_, aiConfig_)
    {
        if (aiConfig_.provider != "stub" && aiConfig_.provider != "ollama" && !aiConfig_.apiKeyConfigured)
        {
            LOG_WARN << "AI provider configured as " << aiConfig_.provider.c_str()
                     << " but CPP_AI_API_KEY or provider API key is not set";
        }

        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的subloop线程数量
        server_.setThreadNum(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->connected())
        {
            LOG_INFO<<"Connection UP :"<<conn->peerAddress().toIpPort().c_str();
        }
        else
        {
            LOG_INFO<<"Connection DOWN :"<<conn->peerAddress().toIpPort().c_str();
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        const std::string request = buf->retrieveAllAsString();
        HttpRequest parsedRequest;
        const auto started = std::chrono::steady_clock::now();
        const RouteResult routeResult = handleRawRequest(request, aiClient_, tools_, &parsedRequest);
        const auto finished = std::chrono::steady_clock::now();
        const long latencyMs = static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count());

        LOG_INFO << "request method=" << parsedRequest.method.c_str()
                 << " path=" << parsedRequest.path.c_str()
                 << " status=" << routeResult.status.c_str()
                 << " body_size=" << parsedRequest.body.size()
                 << " cache_hit=" << (routeResult.cacheHit ? "true" : "false")
                 << " tool_used=" << (routeResult.toolUsed ? "true" : "false")
                 << " tool=" << routeResult.toolName.c_str()
                 << " upstream_ms=" << routeResult.upstreamLatencyMs
                 << " total_ms=" << latencyMs;

        conn->send(routeResult.response);
        conn->shutdown();   // 关闭写端，HTTP/1.0 风格一请求一响应
    }
    TcpServer server_;
    EventLoop *loop_;
    ToolRegistry tools_;
    AiClientConfig aiConfig_;
    AiClient aiClient_;

};
AsyncLogging* g_asyncLog = NULL;
AsyncLogging * getAsyncLog(){
    return g_asyncLog;
}
 void asyncLog(const char* msg, int len)
{
    AsyncLogging* logging = getAsyncLog();
    if (logging)
    {
        logging->append(msg, len);
    }
}
int main(int argc,char *argv[]) {
    // Browser clients may close an HTTP connection before the server finishes
    // writing. Ignore SIGPIPE so one closed socket does not kill the process.
    ::signal(SIGPIPE, SIG_IGN);

    //第一步启动日志，双缓冲异步写入磁盘.
    //创建一个文件夹
    const std::string LogDir="logs";
    mkdir(LogDir.c_str(),0755);
    //使用std::stringstream 构建日志文件夹
    std::ostringstream LogfilePath;
    LogfilePath << LogDir << "/" << ::basename(argv[0]); // 完整的日志文件路径
    AsyncLogging log(LogfilePath.str(), kRollSize);
    g_asyncLog = &log;
    Logger::setOutput(asyncLog); // 为Logger设置输出回调, 重新配接输出位置
    log.start(); // 开启日志后端线程
    //第二步启动内存池和LFU缓存
     // 初始化内存池
    memoryPool::HashBucket::initMemoryPool();

    //第三步启动底层网络模块
    EventLoop loop;
    InetAddress addr(8080);
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
 // 主loop开始事件循环  epoll_wait阻塞 等待就绪事件(主loop只注册了监听套接字的fd，所以只会处理新连接事件)
    std::cout << "================================================Start Web Server================================================" << std::endl;
    loop.loop();
    std::cout << "================================================Stop Web Server=================================================" << std::endl;
    //结束日志打印
    log.stop();
}
