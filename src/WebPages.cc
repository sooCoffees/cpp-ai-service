#include <WebPages.h>
#include <HttpCodec.h>

std::string renderHomePage()
{
    const std::string body = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>QClaw Agent Workspace</title>
<style>
:root {
  color-scheme: dark;
  --bg: #171a16;
  --rail: #101410;
  --sidebar: #292d25;
  --sidebar-2: #353a31;
  --surface: #20241f;
  --surface-2: #2a3028;
  --field: #3b4038;
  --line: rgba(255, 255, 255, 0.12);
  --text: #f7f6ee;
  --muted: #c2c8bd;
  --dim: #899085;
  --accent: #19b6ff;
  --accent-2: #23d18b;
  --danger: #ff6b6b;
  --shadow: 0 18px 80px rgba(0, 0, 0, 0.38);
}
* { box-sizing: border-box; }
.hidden { display: none !important; }
html, body { height: 100%; }
body {
  margin: 0;
  background:
    linear-gradient(135deg, rgba(57, 78, 65, 0.18), transparent 42%),
    radial-gradient(circle at 90% 0%, rgba(54, 139, 160, 0.16), transparent 34%),
    var(--bg);
  color: var(--text);
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}
button, input, textarea, select { font: inherit; }
.app {
  min-height: 100vh;
  display: grid;
  grid-template-columns: 48px 236px minmax(0, 1fr);
}
.rail {
  background: rgba(15, 18, 15, 0.82);
  border-right: 1px solid var(--line);
  padding: 18px 7px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 18px;
}
.profile {
  width: 30px;
  height: 30px;
  border-radius: 50%;
  display: grid;
  place-items: center;
  background: linear-gradient(135deg, #ff7b3f, #ffe18b);
  color: #24120a;
  font-weight: 900;
  font-size: 13px;
}
.rail-nav {
  display: grid;
  gap: 13px;
  width: 100%;
}
.rail-btn {
  width: 34px;
  height: 38px;
  border: 0;
  border-radius: 8px;
  display: grid;
  place-items: center;
  gap: 2px;
  background: transparent;
  color: var(--muted);
  cursor: pointer;
  font-size: 12px;
}
.rail-btn span {
  font-size: 10px;
  line-height: 1;
}
.rail-btn.active {
  background: rgba(255, 255, 255, 0.1);
  color: var(--text);
}
.sidebar {
  border-right: 1px solid var(--line);
  background: rgba(42, 47, 39, 0.88);
  padding: 16px 12px;
  display: flex;
  flex-direction: column;
  gap: 12px;
  min-width: 0;
}
.search {
  position: relative;
}
.search input {
  min-height: 34px;
  border-radius: 999px;
  padding-left: 38px;
  background: rgba(255, 255, 255, 0.13);
  border: 1px solid rgba(255, 255, 255, 0.11);
}
.search::before {
  content: "⌕";
  position: absolute;
  left: 14px;
  top: 6px;
  color: var(--muted);
  font-weight: 800;
}
.new-chat, .send, .chip, .agent-card {
  cursor: pointer;
}
.new-chat {
  width: 100%;
  min-height: 36px;
  border: 0;
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.15);
  color: var(--text);
  font-weight: 700;
}
.agent-card {
  min-height: 54px;
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.16);
  display: grid;
  grid-template-columns: 38px minmax(0, 1fr) 24px;
  gap: 10px;
  align-items: center;
  padding: 8px;
  color: var(--text);
  text-align: left;
}
.agent-card.active {
  border-color: rgba(25, 182, 255, 0.55);
  background: rgba(255, 255, 255, 0.2);
}
.agent-avatar {
  width: 38px;
  height: 38px;
  border-radius: 50%;
  display: grid;
  place-items: center;
  background: linear-gradient(135deg, #e83f4f, #fff8ce 58%, #2fd8ff);
  color: #141713;
  font-weight: 900;
}
.agent-name {
  font-weight: 800;
  font-size: 13px;
}
.agent-desc {
  margin-top: 2px;
  color: var(--muted);
  font-size: 12px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.agent-list {
  display: grid;
  gap: 8px;
}
.agent-item {
  display: grid;
  gap: 8px;
}
.delete-agent {
  width: 24px;
  height: 24px;
  border: 0;
  border-radius: 50%;
  display: grid;
  place-items: center;
  background: rgba(255, 255, 255, 0.12);
  color: var(--muted);
  cursor: pointer;
  font-size: 13px;
  font-weight: 900;
}
.delete-agent:hover {
  background: rgba(255, 107, 107, 0.18);
  color: #ffb8b8;
}
input, select {
  width: 100%;
  min-height: 36px;
  color: var(--text);
  background: rgba(255, 255, 255, 0.08);
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 8px;
  padding: 0 10px;
}
.field {
  display: grid;
  gap: 6px;
  margin-bottom: 8px;
}
.field label {
  color: var(--muted);
  font-size: 12px;
}
.side-panel {
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 8px;
  padding: 10px;
  margin-left: 48px;
  background: rgba(255, 255, 255, 0.07);
}
.side-panel.hidden {
  display: none;
}
.side-panel details {
  border-radius: 8px;
}
.side-panel summary {
  cursor: pointer;
  color: var(--text);
  font-size: 12px;
  font-weight: 800;
}
.hint {
  margin: 8px 0 0;
  color: var(--muted);
  font-size: 12px;
  line-height: 1.45;
}
.main {
  min-width: 0;
  display: grid;
  grid-template-rows: 54px minmax(0, 1fr) auto;
  background:
    linear-gradient(90deg, rgba(255, 255, 255, 0.025), transparent 22%),
    rgba(26, 29, 25, 0.86);
}
.topbar {
  min-height: 54px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 0 18px;
}
.tabs {
  display: flex;
  align-items: center;
  gap: 8px;
}
.chip {
  min-width: 74px;
  min-height: 26px;
  border: 0;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.14);
  color: var(--text);
  font-size: 12px;
  font-weight: 800;
}
.chip.active {
  background: linear-gradient(135deg, #19b6ff, #087bff);
  box-shadow: 0 6px 22px rgba(25, 182, 255, 0.26);
}
.status {
  color: var(--muted);
  font-size: 12px;
}
.status.ok { color: var(--accent); }
.status.error { color: var(--danger); }
.top-actions {
  display: flex;
  align-items: center;
  gap: 12px;
}
.auth-bar {
  display: flex;
  align-items: center;
  gap: 8px;
}
.auth-user {
  max-width: 160px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--muted);
  font-size: 12px;
  font-weight: 800;
}
.auth-button {
  min-height: 30px;
  border: 1px solid rgba(255, 255, 255, 0.14);
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.12);
  color: var(--text);
  padding: 0 10px;
  font-size: 12px;
  font-weight: 800;
  cursor: pointer;
}
.auth-button.primary {
  border-color: rgba(25, 182, 255, 0.5);
  background: rgba(25, 182, 255, 0.18);
}
.auth-modal {
  position: fixed;
  inset: 0;
  z-index: 20;
  display: grid;
  place-items: center;
  background: rgba(0, 0, 0, 0.48);
  padding: 18px;
}
.auth-modal.hidden {
  display: none;
}
.auth-dialog {
  width: min(380px, 100%);
  border: 1px solid rgba(255, 255, 255, 0.16);
  border-radius: 8px;
  background: #20241f;
  box-shadow: var(--shadow);
  padding: 14px;
}
.auth-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 12px;
}
.auth-title {
  margin: 0;
  font-size: 16px;
}
.auth-close {
  width: 30px;
  height: 30px;
  border: 0;
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.1);
  color: var(--muted);
  cursor: pointer;
  font-weight: 900;
}
.auth-tabs {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 6px;
  margin-bottom: 12px;
}
.auth-tab {
  min-height: 32px;
  border: 0;
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.1);
  color: var(--muted);
  cursor: pointer;
  font-weight: 800;
}
.auth-tab.active {
  background: rgba(25, 182, 255, 0.2);
  color: var(--text);
}
.auth-form {
  display: grid;
  gap: 10px;
}
.auth-form.hidden {
  display: none;
}
.auth-submit {
  min-height: 36px;
  border: 0;
  border-radius: 8px;
  background: linear-gradient(135deg, #19b6ff, #087bff);
  color: white;
  font-weight: 900;
  cursor: pointer;
}
.auth-note {
  min-height: 18px;
  margin: 0;
  color: var(--muted);
  font-size: 12px;
  line-height: 1.45;
}
.auth-note.error {
  color: var(--danger);
}
.auth-note.ok {
  color: var(--accent-2);
}
.workspace {
  min-height: 0;
  overflow-y: auto;
  padding: 54px 18px 26px;
}
.hero {
  width: min(920px, 100%);
  margin: 96px auto 42px;
  text-align: center;
}
.hero h1 {
  margin: 0 0 8px;
  font-size: clamp(26px, 4vw, 36px);
  line-height: 1.15;
  letter-spacing: 0;
}
.hero h1 .mark {
  display: inline-block;
  position: relative;
}
.hero h1 .mark::after {
  content: "";
  position: absolute;
  left: 3px;
  right: 3px;
  bottom: 1px;
  height: 7px;
  border-radius: 999px;
  background: rgba(255, 69, 94, 0.78);
  z-index: -1;
}
.hero p {
  margin: 0;
  color: var(--text);
  font-weight: 800;
  font-size: 16px;
}
.conversation {
  width: min(920px, 100%);
  margin: 0 auto;
}
.messages {
  min-height: 0;
  margin-bottom: 18px;
}
.message {
  margin: 0 0 14px;
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
  background: var(--surface-2);
  color: var(--muted);
  font-size: 13px;
  font-weight: 800;
}
.assistant .avatar {
  background: rgba(25, 182, 255, 0.18);
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
  background: rgba(255, 255, 255, 0.08);
  border: 1px solid rgba(255, 255, 255, 0.12);
  border-radius: 8px;
  padding: 14px 16px;
  box-shadow: var(--shadow);
}
.composer {
  padding: 0 18px 24px;
  background: linear-gradient(180deg, transparent, rgba(23, 26, 22, 0.76));
}
.composer-inner {
  width: min(920px, 100%);
  margin: 0 auto;
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  gap: 8px;
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.18);
  border-radius: 18px;
  padding: 13px 14px 11px;
  box-shadow: 0 18px 80px rgba(0, 0, 0, 0.32);
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
  padding: 4px 2px 0;
}
.composer-actions {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
}
.mode-tools {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
}
.mini-select {
  width: auto;
  min-width: 88px;
  min-height: 28px;
  border-radius: 999px;
  font-size: 12px;
  font-weight: 800;
  background: rgba(255, 255, 255, 0.09);
}
.send {
  border: 0;
  width: 32px;
  height: 32px;
  border-radius: 50%;
  display: grid;
  place-items: center;
  background: rgba(255, 255, 255, 0.84);
  color: #22251f;
  font-size: 17px;
  font-weight: 900;
}
.send:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}
@media (max-width: 760px) {
  .app { grid-template-columns: 44px minmax(0, 1fr); }
  .sidebar {
    display: none;
  }
}
@media (max-width: 640px) {
  .app { grid-template-columns: 1fr; }
  .rail { display: none; }
  .topbar {
    padding: 10px 12px;
    min-height: 58px;
    align-items: flex-start;
    flex-direction: column;
  }
  .workspace {
    padding: 30px 14px 18px;
  }
  .composer {
    padding: 0 12px 14px;
  }
}
</style>
</head>
<body>
<div class="app">
  <nav class="rail" aria-label="Primary">
    <div class="profile">Q</div>
    <div class="rail-nav">
      <button class="rail-btn active" type="button" title="Chat">◎<span>Chat</span></button>
      <button class="rail-btn" type="button" title="Settings">⚙<span>Setup</span></button>
    </div>
  </nav>
  <aside class="sidebar">
    <div class="search">
      <input id="searchInput" spellcheck="false" placeholder="Search">
    </div>
    <button class="new-chat" id="newChat">+ New Agent</button>
    <div class="agent-list" id="agentList"></div>

    <section class="side-panel hidden" id="providerPanel">
      <details>
        <summary>Provider</summary>
        <div class="field">
          <label for="preset">Preset</label>
          <select id="preset">
            <option value="https://api.chatanywhere.tech/v1|gpt-3.5-turbo">ChatAnywhere Demo</option>
            <option value="https://api.openai.com/v1|gpt-4.1-mini">OpenAI</option>
            <option value="https://api.deepseek.com/v1|deepseek-chat">DeepSeek</option>
            <option value="https://openrouter.ai/api/v1|openrouter/free">OpenRouter Free</option>
            <option value="https://api.groq.com/openai/v1|llama-3.1-8b-instant">Groq</option>
            <option value="custom|">Custom</option>
          </select>
        </div>
        <div class="field">
          <label for="baseUrl">Base URL</label>
          <input id="baseUrl" spellcheck="false" value="https://api.chatanywhere.tech/v1">
        </div>
        <div class="field">
          <label for="model">Model</label>
          <input id="model" spellcheck="false" value="gpt-3.5-turbo">
        </div>
        <div class="field">
          <label for="apiKey">API Key</label>
          <input id="apiKey" type="password" spellcheck="false" placeholder="sk-...">
        </div>
      </details>
    </section>
  </aside>

  <main class="main">
    <header class="topbar">
      <div class="tabs" aria-label="Workspace modes">
        <button class="chip active" type="button">Chat</button>
        <button class="chip" type="button">Workspace</button>
        <button class="chip" type="button">Agents</button>
      </div>
      <div class="top-actions">
        <span class="status" id="status">Checking service...</span>
        <div class="auth-bar">
          <span class="auth-user" id="authUser">Guest</span>
          <button class="auth-button primary" id="authOpen" type="button">Sign in</button>
          <button class="auth-button hidden" id="authLogout" type="button">Sign out</button>
        </div>
      </div>
    </header>

    <section class="workspace" id="workspace">
      <div class="hero" id="welcome">
        <h1>Hi, I am <span class="mark">QClaw</span></h1>
        <p>Build agents, connect tools, and get work done faster.</p>
      </div>

      <div class="conversation">
        <section class="messages" id="messages"></section>
      </div>
    </section>

    <form class="composer" id="chatForm">
      <div class="composer-inner">
        <textarea id="messageInput" rows="1" placeholder="Ask anything or describe a task"></textarea>
        <div class="composer-actions">
          <div class="mode-tools">
            <select class="mini-select" id="modeSelect" title="Chat mode">
              <option value="auto">Auto</option>
              <option value="model">Model</option>
              <option value="rag">RAG</option>
              <option value="mcp">MCP</option>
            </select>
            <select class="mini-select" id="toolMirror" title="Gateway tool">
              <option value="">Connect</option>
            </select>
          </div>
          <button class="send" id="sendButton" type="submit" title="Send">↑</button>
        </div>
      </div>
    </form>
  </main>
</div>
<div class="auth-modal hidden" id="authModal" aria-hidden="true">
  <div class="auth-dialog" role="dialog" aria-modal="true" aria-labelledby="authTitle">
    <div class="auth-head">
      <h2 class="auth-title" id="authTitle">Account</h2>
      <button class="auth-close" id="authClose" type="button" title="Close">x</button>
    </div>
    <div class="auth-tabs">
      <button class="auth-tab active" id="loginTab" type="button">Login</button>
      <button class="auth-tab" id="registerTab" type="button">Register</button>
    </div>
    <form class="auth-form" id="loginForm">
      <input id="loginUsername" autocomplete="username" spellcheck="false" placeholder="Username">
      <input id="loginPassword" type="password" autocomplete="current-password" placeholder="Password">
      <button class="auth-submit" type="submit">Login</button>
    </form>
    <form class="auth-form hidden" id="registerForm">
      <input id="registerUsername" autocomplete="username" spellcheck="false" placeholder="Username">
      <input id="registerPassword" type="password" autocomplete="new-password" placeholder="Password">
      <input id="registerInvite" spellcheck="false" placeholder="Invite code, if required">
      <button class="auth-submit" type="submit">Register</button>
    </form>
    <p class="auth-note" id="authNote"></p>
  </div>
</div>
<script>
const messages = document.getElementById('messages');
const workspace = document.getElementById('workspace');
const welcome = document.getElementById('welcome');
const form = document.getElementById('chatForm');
const input = document.getElementById('messageInput');
const sendButton = document.getElementById('sendButton');
const statusEl = document.getElementById('status');
const toolMirror = document.getElementById('toolMirror');
const modeSelect = document.getElementById('modeSelect');
const newChat = document.getElementById('newChat');
const agentList = document.getElementById('agentList');
const providerPanel = document.getElementById('providerPanel');
const preset = document.getElementById('preset');
const baseUrl = document.getElementById('baseUrl');
const model = document.getElementById('model');
const apiKey = document.getElementById('apiKey');
const authModal = document.getElementById('authModal');
const authOpen = document.getElementById('authOpen');
const authClose = document.getElementById('authClose');
const authLogout = document.getElementById('authLogout');
const authUser = document.getElementById('authUser');
const loginTab = document.getElementById('loginTab');
const registerTab = document.getElementById('registerTab');
const loginForm = document.getElementById('loginForm');
const registerForm = document.getElementById('registerForm');
const loginUsername = document.getElementById('loginUsername');
const loginPassword = document.getElementById('loginPassword');
const registerUsername = document.getElementById('registerUsername');
const registerPassword = document.getElementById('registerPassword');
const registerInvite = document.getElementById('registerInvite');
const authNote = document.getElementById('authNote');
const history = [];
const agents = [
  { id: 'qclaw', name: 'QClaw', description: 'Your always-on AI workspace' }
];
let activeAgentId = 'qclaw';
let providerPanelOpen = false;
let currentUser = null;

function showProviderPanel() {
  providerPanelOpen = true;
  providerPanel.classList.remove('hidden');
}

function setStatus(text, state) {
  statusEl.textContent = text;
  statusEl.className = 'status' + (state ? ' ' + state : '');
}

function setAuthNote(text, state) {
  authNote.textContent = text || '';
  authNote.className = 'auth-note' + (state ? ' ' + state : '');
}

function authToken() {
  return localStorage.getItem('cppAiSessionToken') || '';
}

function setAuthToken(token) {
  if (token) {
    localStorage.setItem('cppAiSessionToken', token);
  } else {
    localStorage.removeItem('cppAiSessionToken');
  }
}

function renderAuth() {
  if (currentUser) {
    authUser.textContent = currentUser.username;
    authOpen.classList.add('hidden');
    authLogout.classList.remove('hidden');
  } else {
    authUser.textContent = 'Guest';
    authOpen.classList.remove('hidden');
    authLogout.classList.add('hidden');
  }
}

function openAuthModal(mode) {
  authModal.classList.remove('hidden');
  authModal.setAttribute('aria-hidden', 'false');
  switchAuthMode(mode || 'login');
  setAuthNote('');
  setTimeout(() => {
    if (mode === 'register') registerUsername.focus();
    else loginUsername.focus();
  }, 0);
}

function closeAuthModal() {
  authModal.classList.add('hidden');
  authModal.setAttribute('aria-hidden', 'true');
}

function switchAuthMode(mode) {
  const isRegister = mode === 'register';
  loginTab.classList.toggle('active', !isRegister);
  registerTab.classList.toggle('active', isRegister);
  loginForm.classList.toggle('hidden', isRegister);
  registerForm.classList.toggle('hidden', !isRegister);
  setAuthNote('');
}

async function readJsonResponse(res) {
  const raw = await res.text();
  try {
    return JSON.parse(raw);
  } catch (err) {
    throw new Error('Non-JSON response: ' + raw.slice(0, 120));
  }
}

async function refreshCurrentUser() {
  const token = authToken();
  if (!token) {
    currentUser = null;
    renderAuth();
    return;
  }

  try {
    const res = await fetch('/auth/me', {
      headers: { 'Authorization': 'Bearer ' + token }
    });
    const data = await readJsonResponse(res);
    if (!res.ok || data.ok === false) throw new Error(data.error || ('HTTP ' + res.status));
    currentUser = data.user;
  } catch (err) {
    currentUser = null;
    setAuthToken('');
  }
  renderAuth();
}

async function submitLogin(username, password) {
  const res = await fetch('/auth/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password })
  });
  const data = await readJsonResponse(res);
  if (!res.ok || data.ok === false) throw new Error(data.error || ('HTTP ' + res.status));
  setAuthToken(data.session_token || '');
  currentUser = data.user;
  renderAuth();
  closeAuthModal();
  setStatus('Signed in as ' + currentUser.username, 'ok');
}

async function submitRegister(username, password, inviteCode) {
  const payload = { username, password };
  if (inviteCode) payload.invite_code = inviteCode;
  const res = await fetch('/auth/register', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  const data = await readJsonResponse(res);
  if (!res.ok || data.ok === false) throw new Error(data.error || ('HTTP ' + res.status));
  await submitLogin(username, password);
}

function addMessage(role, text) {
  welcome.style.display = 'none';
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
  workspace.scrollTop = workspace.scrollHeight;
  return bubble;
}

function resetChat() {
  messages.innerHTML = '';
  history.length = 0;
  welcome.style.display = '';
  input.focus();
}

function activeAgent() {
  return agents.find((agent) => agent.id === activeAgentId) || agents[0];
}

function updateWelcome() {
  const agent = activeAgent();
  const mark = welcome.querySelector('.mark');
  if (mark) mark.textContent = agent.name;
}

function selectAgent(agentId) {
  activeAgentId = agentId;
  showProviderPanel();
  renderAgents();
  resetChat();
  updateWelcome();
}

function createAgentCard(agent) {
  const card = document.createElement('div');
  card.className = 'agent-card' + (agent.id === activeAgentId ? ' active' : '');
  card.dataset.agentId = agent.id;

  const avatar = document.createElement('div');
  avatar.className = 'agent-avatar';
  avatar.textContent = agent.name.trim().charAt(0).toUpperCase() || 'A';

  const body = document.createElement('div');
  const name = document.createElement('div');
  name.className = 'agent-name';
  name.textContent = agent.name;
  const desc = document.createElement('div');
  desc.className = 'agent-desc';
  desc.textContent = agent.description;

  body.appendChild(name);
  body.appendChild(desc);
  const deleteButton = document.createElement('button');
  deleteButton.className = 'delete-agent';
  deleteButton.type = 'button';
  deleteButton.title = 'Delete agent';
  deleteButton.textContent = 'x';
  deleteButton.addEventListener('click', (event) => {
    event.stopPropagation();
    deleteAgent(agent.id);
  });

  card.appendChild(avatar);
  card.appendChild(body);
  card.appendChild(deleteButton);
  card.addEventListener('click', () => selectAgent(agent.id));
  return card;
}

function renderAgents() {
  providerPanel.remove();
  agentList.innerHTML = '';
  agents.forEach((agent) => {
    const item = document.createElement('div');
    item.className = 'agent-item';
    item.appendChild(createAgentCard(agent));
    if (providerPanelOpen && agent.id === activeAgentId) {
      providerPanel.classList.remove('hidden');
      item.appendChild(providerPanel);
    }
    agentList.appendChild(item);
  });
}

function addAgent() {
  const nextNumber = agents.length + 1;
  const agent = {
    id: 'agent-' + nextNumber + '-' + Date.now(),
    name: 'Agent ' + nextNumber,
    description: 'New private workspace'
  };
  agents.push(agent);
  selectAgent(agent.id);
}

function deleteAgent(agentId) {
  if (agents.length <= 1) return;
  const index = agents.findIndex((agent) => agent.id === agentId);
  if (index === -1) return;
  agents.splice(index, 1);
  if (activeAgentId === agentId) {
    activeAgentId = agents[Math.max(0, index - 1)].id;
    resetChat();
    updateWelcome();
  }
  renderAgents();
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

function appendToolOption(select, value, label, title) {
  const option = document.createElement('option');
  option.value = value;
  option.textContent = label;
  option.title = title || '';
  select.appendChild(option);
}

async function loadTools() {
  try {
    const res = await fetch('/tools');
    const data = await res.json();
    if (!data.ok || !Array.isArray(data.tools)) return;
    data.tools.forEach((tool) => {
      appendToolOption(toolMirror, tool.name, tool.name, tool.description || '');
    });
  } catch (err) {
    // Tools are optional for the UI.
  }
}

function saveProviderConfig() {
  localStorage.setItem('homeApi.baseUrl', baseUrl.value.trim());
  localStorage.setItem('homeApi.model', model.value.trim());
}

function loadProviderConfig() {
  baseUrl.value = localStorage.getItem('homeApi.baseUrl') || baseUrl.value;
  model.value = localStorage.getItem('homeApi.model') || model.value;
}

function providerUrl() {
  return baseUrl.value.trim().replace(/\/+$/, '') + '/chat/completions';
}

async function sendDirect(text) {
  const key = apiKey.value.trim();
  if (!key) {
    throw new Error('API key is required for direct provider chat');
  }

  const messagesForApi = history.concat([{ role: 'user', content: text }]);
  const res = await fetch(providerUrl(), {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': 'Bearer ' + key
    },
    body: JSON.stringify({
      model: model.value.trim(),
      messages: messagesForApi,
      temperature: 0.2
    })
  });

  const raw = await res.text();
  let data;
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

async function sendMessage(text) {
  saveProviderConfig();
  const selectedTool = toolMirror.value;
  if (!selectedTool) {
    return sendDirect(text);
  }

  const payload = { message: text, tool: selectedTool };
  const headers = { 'Content-Type': 'application/json' };
  const token = authToken();
  if (token) headers.Authorization = 'Bearer ' + token;
  const res = await fetch('/chat', {
    method: 'POST',
    headers,
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

preset.addEventListener('change', () => {
  const value = preset.value;
  if (value === 'custom|') return;
  const parts = value.split('|');
  baseUrl.value = parts[0] || baseUrl.value;
  model.value = parts[1] || model.value;
  saveProviderConfig();
});

toolMirror.addEventListener('change', () => {
  if (toolMirror.value) modeSelect.value = 'mcp';
});

form.addEventListener('submit', async (event) => {
  event.preventDefault();
  const text = input.value.trim();
  if (!text) return;

  addMessage('user', text);
  input.value = '';
  resizeInput();
  input.disabled = true;
  sendButton.disabled = true;
  const selectedTool = toolMirror.value;
  setStatus(selectedTool ? 'Calling local gateway...' : 'Calling provider...');
  const pending = addMessage('assistant', '...');

  try {
    pending.textContent = await sendMessage(text);
    setStatus(selectedTool ? 'Local gateway mode' : 'Direct API mode', 'ok');
  } catch (err) {
    pending.textContent = 'Request failed: ' + err.message + '\n\nIf browser direct mode is blocked by CORS, select a local tool or run the service with provider env vars.';
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
newChat.addEventListener('click', addAgent);
authOpen.addEventListener('click', () => openAuthModal('login'));
authClose.addEventListener('click', closeAuthModal);
authModal.addEventListener('click', (event) => {
  if (event.target === authModal) closeAuthModal();
});
loginTab.addEventListener('click', () => switchAuthMode('login'));
registerTab.addEventListener('click', () => switchAuthMode('register'));
authLogout.addEventListener('click', () => {
  setAuthToken('');
  currentUser = null;
  renderAuth();
  setStatus('Signed out', 'ok');
});
loginForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  setAuthNote('Signing in...');
  try {
    await submitLogin(loginUsername.value.trim(), loginPassword.value);
  } catch (err) {
    setAuthNote(err.message, 'error');
  }
});
registerForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  setAuthNote('Creating account...');
  try {
    await submitRegister(registerUsername.value.trim(), registerPassword.value, registerInvite.value.trim());
  } catch (err) {
    setAuthNote(err.message, 'error');
  }
});

renderAgents();
updateWelcome();
resetChat();
resizeInput();
loadProviderConfig();
loadHealth();
loadTools();
refreshCurrentUser();
</script>
</body>
</html>)HTML";
    return httpResponse("200 OK", "text/html; charset=utf-8", body);
}

std::string renderDirectApiPage()
{
    const std::string body = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>API Playground</title>
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
    <div class="brand"><div class="logo">API</div><span>API Playground</span></div>
    <p class="hint">Use ChatAnywhere as the demo provider, or choose Custom and bring your own OpenAI-compatible Base URL, Model, and API Key.</p>

    <label>Preset
      <select id="preset">
        <option value="https://api.chatanywhere.tech/v1|gpt-3.5-turbo">ChatAnywhere Demo</option>
        <option value="https://api.openai.com/v1|gpt-4.1-mini">OpenAI</option>
        <option value="https://api.deepseek.com/v1|deepseek-chat">DeepSeek</option>
        <option value="https://openrouter.ai/api/v1|openrouter/free">OpenRouter Free</option>
        <option value="https://api.groq.com/openai/v1|llama-3.1-8b-instant">Groq</option>
        <option value="http://127.0.0.1:11434/v1|llama3.2">Ollama local</option>
        <option value="custom|">Custom</option>
      </select>
    </label>

    <label>Base URL
      <input id="baseUrl" spellcheck="false" value="https://api.chatanywhere.tech/v1">
    </label>

    <label>Model
      <input id="model" spellcheck="false" value="gpt-3.5-turbo">
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
    <p class="hint">Browser mode is for demos. For real usage, keep keys server-side and use local gateway mode.</p>
  </aside>

  <main class="main">
    <header class="topbar">
      <div class="title">
        <strong>Chat</strong>
        <span class="status" id="status">Demo browser mode</span>
      </div>
    </header>
    <section class="messages" id="messages"></section>
    <form class="composer" id="chatForm">
      <div class="composer-inner">
        <textarea id="messageInput" rows="1" placeholder="Message API playground"></textarea>
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
  addMessage('assistant', 'Use ChatAnywhere as the demo provider, or select Custom and enter your own OpenAI-compatible API settings.');
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
