#include <ToolRegistry.h>
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
                                ToolHandler handler)
{
    tools_[name] = ToolEntry{description, handler};
}

bool ToolRegistry::hasTool(const std::string &name) const
{
    return tools_.find(name) != tools_.end();
}

ToolResult ToolRegistry::execute(const std::string &name) const
{
    auto it = tools_.find(name);
    if (it == tools_.end())
    {
        return ToolResult{false, "", "unknown tool: " + name};
    }

    return it->second.handler();
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
             << "\"input_schema\":{"
             << "\"type\":\"object\","
             << "\"properties\":{},"
             << "\"additionalProperties\":false"
             << "}"
             << "}";
    }

    body << "]}";
    return body.str();
}

ToolRegistry ToolRegistry::createDefault()
{
    ToolRegistry registry;

    registry.registerTool(
        "project_status",
        "Return the current cpp-ai-service project status.",
        []() {
            const std::string json =
                "{"
                "\"project\":\"cpp-ai-service\","
                "\"status\":\"C++ AI service gateway skeleton\","
                "\"features\":[\"/health\",\"/chat\",\"AiClient\",\"ToolRegistry\",\"LFU response cache\",\"environment config\",\"OpenAI-compatible provider\"],"
                "\"next\":\"model-driven tool calling\""
                "}";
            return ToolResult{true, json, ""};
        });

    registry.registerTool(
        "server_time",
        "Return the server timestamp.",
        []() {
            std::ostringstream json;
            json << "{"
                 << "\"timestamp\":\"" << jsonEscape(Timestamp::now().toFormattedString(true)) << "\""
                 << "}";
            return ToolResult{true, json.str(), ""};
        });

    return registry;
}
