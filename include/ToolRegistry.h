#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

struct ToolResult
{
    bool ok;
    std::string json;
    std::string error;
};

class ToolRegistry
{
public:
    using ToolHandler = std::function<ToolResult()>;

    struct ToolInfo
    {
        std::string name;
        std::string description;
    };

    void registerTool(const std::string &name,
                      const std::string &description,
                      ToolHandler handler);

    bool hasTool(const std::string &name) const;
    ToolResult execute(const std::string &name) const;
    std::string listToolsJson() const;
    std::string listMcpToolsJson() const;

    static ToolRegistry createDefault();

private:
    struct ToolEntry
    {
        std::string description;
        ToolHandler handler;
    };

    std::map<std::string, ToolEntry> tools_;
};
