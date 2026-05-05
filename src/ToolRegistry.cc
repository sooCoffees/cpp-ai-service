#include <ToolRegistry.h>
#include <RagStore.h>
#include <Timestamp.h>

#include <sstream>

namespace
{
std::string jsonEscape(const std::string &input)
{
    std::string output;
    output.reserve(input.size());
    for (char ch : input)
    {
        switch (ch)
        {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += ch;
            break;
        }
    }
    return output;
}
}

void ToolRegistry::registerTool(const std::string &name,
                                const std::string &description,
                                const std::string &inputSchema,
                                ToolHandler handler)
{
    tools_[name] = ToolEntry{description, inputSchema, handler};
}

bool ToolRegistry::hasTool(const std::string &name) const
{
    return tools_.find(name) != tools_.end();
}

ToolResult ToolRegistry::execute(const std::string &name) const
{
    return execute(name, "");
}

ToolResult ToolRegistry::execute(const std::string &name, const std::string &input) const
{
    auto it = tools_.find(name);
    if (it == tools_.end())
    {
        return ToolResult{false, "", "unknown tool: " + name};
    }

    return it->second.handler(input);
}

std::string ToolRegistry::listToolsJson() const
{
    std::ostringstream body;
    body << "{\"ok\":true,\"tools\":[";

    bool first = true;
    for (const auto &item : tools_)
    {
        if (!first)
        {
            body << ",";
        }
        first = false;
        body << "{"
             << "\"name\":\"" << jsonEscape(item.first) << "\","
             << "\"description\":\"" << jsonEscape(item.second.description) << "\""
             << "}";
    }

    body << "]}";
    return body.str();
}

std::string ToolRegistry::listMcpToolsJson() const
{
    std::ostringstream body;
    body << "{\"ok\":true,\"protocol\":\"mcp-like\",\"tools\":[";

    bool first = true;
    for (const auto &item : tools_)
    {
        if (!first)
        {
            body << ",";
        }
        first = false;
        body << "{"
             << "\"name\":\"" << jsonEscape(item.first) << "\","
             << "\"description\":\"" << jsonEscape(item.second.description) << "\","
             << "\"input_schema\":" << item.second.inputSchema
             << "}";
    }

    body << "]}";
    return body.str();
}

ToolRegistry ToolRegistry::createDefault(std::shared_ptr<InMemoryRagStore> ragStore)
{
    ToolRegistry registry;

    registry.registerTool(
        "project_status",
        "Return the current cpp-ai-service project status.",
        "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}",
        [](const std::string &) {
            const std::string json =
                "{"
                "\"project\":\"cpp-ai-service\","
                "\"status\":\"C++ AI service gateway with tools, cache, MCP-like endpoints, and minimal RAG\","
                "\"features\":[\"/health\",\"/chat\",\"AiClient\",\"ToolRegistry\",\"LFU response cache\",\"environment config\",\"OpenAI-compatible provider\",\"/mcp/tools\",\"/mcp/call\",\"rag_search\"],"
                "\"next\":\"full MCP JSON-RPC and persistent vector store\""
                "}";
            return ToolResult{true, json, ""};
        });

    registry.registerTool(
        "server_time",
        "Return the server timestamp.",
        "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}",
        [](const std::string &) {
            std::ostringstream json;
            json << "{"
                 << "\"timestamp\":\"" << jsonEscape(Timestamp::now().toFormattedString(true)) << "\""
                 << "}";
            return ToolResult{true, json.str(), ""};
        });

    registry.registerTool(
        "rag_search",
        "Search the local in-memory RAG document store and return matched context.",
        "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"Search query. If omitted, /chat message is used.\"}},\"additionalProperties\":false}",
        [ragStore](const std::string &input) {
            if (!ragStore)
            {
                return ToolResult{false, "", "RAG store is not configured"};
            }

            const std::string query = input.empty() ? "cpp-ai-service" : input;
            return ToolResult{true, ragStore->searchJson(query, 3), ""};
        });

    return registry;
}
