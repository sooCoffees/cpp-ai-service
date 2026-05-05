#pragma once

#include <memory>
#include <string>
#include <vector>

struct RagDocument
{
    std::string id;
    std::string title;
    std::string content;
};

struct RagSearchResult
{
    RagDocument document;
    double score;
};

class EmbeddingProvider
{
public:
    virtual ~EmbeddingProvider() {}
    virtual std::vector<double> embed(const std::string &text) const = 0;
};

class KeywordEmbeddingProvider : public EmbeddingProvider
{
public:
    std::vector<double> embed(const std::string &text) const override;
};

class VectorStore
{
public:
    virtual ~VectorStore() {}
    virtual void upsert(const RagDocument &document) = 0;
    virtual std::vector<RagSearchResult> search(const std::string &query, size_t limit) const = 0;
};

class InMemoryRagStore : public VectorStore
{
public:
    explicit InMemoryRagStore(std::shared_ptr<EmbeddingProvider> embeddingProvider =
                                  std::shared_ptr<EmbeddingProvider>());

    void seedDefaults();
    void upsert(const RagDocument &document) override;
    std::vector<RagSearchResult> search(const std::string &query, size_t limit) const override;
    std::string searchJson(const std::string &query, size_t limit) const;

private:
    std::shared_ptr<EmbeddingProvider> embeddingProvider_;
    std::vector<RagDocument> documents_;
};
