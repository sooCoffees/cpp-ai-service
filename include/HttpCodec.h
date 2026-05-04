#pragma once

#include <map>
#include <string>

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string toLower(std::string value);
std::string trim(const std::string &value);
std::string jsonEscape(const std::string &input);
bool parseHttpRequest(const std::string &raw, HttpRequest *request);
bool extractJsonStringField(const std::string &json, const std::string &field, std::string *value);
std::string jsonError(const std::string &message);
std::string httpResponse(const std::string &status,
                         const std::string &contentType,
                         const std::string &body);
std::string jsonResponse(const std::string &status, const std::string &body);
