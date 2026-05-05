#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class InMemoryRagStore;

struct ToolResult
{
    bool ok;
    std::string json;
    std::string error;
};

class ToolRegistry
{
public:
    using ToolHandler = std::function<ToolResult(const std::string &input)>;

    struct ToolInfo
    {
        std::string name;
        std::string description;
        std::string inputSchema;
    };

    void registerTool(const std::string &name,
                      const std::string &description,
                      const std::string &inputSchema,
                      ToolHandler handler);

    bool hasTool(const std::string &name) const;
    ToolResult execute(const std::string &name, const std::string &input) const;
    ToolResult execute(const std::string &name) const;
    std::string listToolsJson() const;
    std::string listMcpToolsJson() const;

    static ToolRegistry createDefault(std::shared_ptr<InMemoryRagStore> ragStore = std::shared_ptr<InMemoryRagStore>());

private:
    struct ToolEntry
    {
        std::string description;
        std::string inputSchema;
        ToolHandler handler;
    };

    std::map<std::string, ToolEntry> tools_;
};
