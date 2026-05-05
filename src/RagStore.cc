#include <RagStore.h>
#include <HttpCodec.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>

namespace
{
std::string lowerCopy(std::string value)
{
    for (char &ch : value)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::vector<std::string> tokenize(const std::string &text)
{
    std::vector<std::string> tokens;
    std::string current;

    for (char ch : lowerCopy(text))
    {
        if (std::isalnum(static_cast<unsigned char>(ch)))
        {
            current += ch;
            continue;
        }

        if (!current.empty())
        {
            tokens.push_back(current);
            current.clear();
        }
    }

    if (!current.empty())
    {
        tokens.push_back(current);
    }

    return tokens;
}

double dotProduct(const std::vector<double> &left, const std::vector<double> &right)
{
    const size_t count = std::min(left.size(), right.size());
    double score = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        score += left[i] * right[i];
    }
    return score;
}
}

std::vector<double> KeywordEmbeddingProvider::embed(const std::string &text) const
{
    static const char *vocabulary[] = {
        "cpp", "service", "gateway", "chat", "tool", "mcp", "rag", "cache",
        "provider", "model", "http", "reactor", "poller", "supabase", "vector"
    };

    std::map<std::string, int> counts;
    for (const std::string &token : tokenize(text))
    {
        counts[token] += 1;
    }

    std::vector<double> vector;
    for (const char *word : vocabulary)
    {
        vector.push_back(static_cast<double>(counts[word]));
    }
    return vector;
}

InMemoryRagStore::InMemoryRagStore(std::shared_ptr<EmbeddingProvider> embeddingProvider)
    : embeddingProvider_(embeddingProvider ? embeddingProvider
                                           : std::shared_ptr<EmbeddingProvider>(new KeywordEmbeddingProvider()))
{
}

void InMemoryRagStore::seedDefaults()
{
    upsert(RagDocument{
        "architecture",
        "C++ AI service gateway architecture",
        "cpp-ai-service is a C++ AI service gateway. HTTP requests flow through TcpServer, "
        "TcpConnection, HttpCodec, AiClient, ToolRegistry, LFU cache, and optional provider calls."
    });
    upsert(RagDocument{
        "mcp",
        "MCP-like tool endpoint",
        "The current MCP-like endpoint supports GET /mcp/tools and POST /mcp/call. "
        "It exposes local tool schemas but is not full MCP JSON-RPC yet."
    });
    upsert(RagDocument{
        "rag",
        "RAG gateway plan",
        "RAG should be a gateway capability: ingest documents, chunk content, create embeddings, "
        "store vectors, retrieve context through rag_search, and then let AiClient generate an answer."
    });
}

void InMemoryRagStore::upsert(const RagDocument &document)
{
    for (RagDocument &existing : documents_)
    {
        if (existing.id == document.id)
        {
            existing = document;
            return;
        }
    }
    documents_.push_back(document);
}

std::vector<RagSearchResult> InMemoryRagStore::search(const std::string &query, size_t limit) const
{
    std::vector<RagSearchResult> results;
    const std::vector<double> queryVector = embeddingProvider_->embed(query);
    const std::vector<std::string> queryTokenList = tokenize(query);
    const std::set<std::string> queryTokens(queryTokenList.begin(), queryTokenList.end());

    for (const RagDocument &document : documents_)
    {
        const std::string haystack = document.title + " " + document.content;
        double score = dotProduct(queryVector, embeddingProvider_->embed(haystack));

        for (const std::string &token : tokenize(haystack))
        {
            if (queryTokens.find(token) != queryTokens.end())
            {
                score += 1.0;
            }
        }

        if (score > 0.0)
        {
            results.push_back(RagSearchResult{document, score});
        }
    }

    std::sort(results.begin(), results.end(), [](const RagSearchResult &left, const RagSearchResult &right) {
        return left.score > right.score;
    });

    if (results.size() > limit)
    {
        results.resize(limit);
    }
    return results;
}

std::string InMemoryRagStore::searchJson(const std::string &query, size_t limit) const
{
    const std::vector<RagSearchResult> results = search(query, limit);
    std::ostringstream body;
    body << "{"
         << "\"query\":\"" << jsonEscape(query) << "\","
         << "\"count\":" << results.size() << ","
         << "\"matches\":[";

    bool first = true;
    for (const RagSearchResult &result : results)
    {
        if (!first)
        {
            body << ",";
        }
        first = false;
        body << "{"
             << "\"id\":\"" << jsonEscape(result.document.id) << "\","
             << "\"title\":\"" << jsonEscape(result.document.title) << "\","
             << "\"content\":\"" << jsonEscape(result.document.content) << "\","
             << "\"score\":" << result.score
             << "}";
    }

    body << "]}";
    return body.str();
}
