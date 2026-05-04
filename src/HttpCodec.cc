#include <HttpCodec.h>

#include <cctype>
#include <cstdlib>
#include <sstream>

std::string toLower(std::string value)
{
    for (char &ch : value)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string trim(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return value.substr(begin, end - begin);
}

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

bool parseHttpRequest(const std::string &raw, HttpRequest *request)
{
    const size_t requestLineEnd = raw.find("\r\n");
    if (requestLineEnd == std::string::npos)
    {
        return false;
    }

    std::istringstream requestLine(raw.substr(0, requestLineEnd));
    if (!(requestLine >> request->method >> request->path >> request->version))
    {
        return false;
    }

    const size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        return false;
    }

    size_t lineStart = requestLineEnd + 2;
    while (lineStart < headerEnd)
    {
        const size_t lineEnd = raw.find("\r\n", lineStart);
        if (lineEnd == std::string::npos || lineEnd > headerEnd)
        {
            return false;
        }

        const std::string line = raw.substr(lineStart, lineEnd - lineStart);
        const size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            request->headers[toLower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
        }
        lineStart = lineEnd + 2;
    }

    request->body = raw.substr(headerEnd + 4);
    auto contentLength = request->headers.find("content-length");
    if (contentLength != request->headers.end())
    {
        const size_t expected = static_cast<size_t>(std::strtoul(contentLength->second.c_str(), nullptr, 10));
        if (request->body.size() > expected)
        {
            request->body.resize(expected);
        }
    }

    return true;
}

bool extractJsonStringField(const std::string &json, const std::string &field, std::string *value)
{
    const std::string key = "\"" + field + "\"";
    const size_t keyPos = json.find(key);
    if (keyPos == std::string::npos)
    {
        return false;
    }

    const size_t colon = json.find(':', keyPos + key.size());
    if (colon == std::string::npos)
    {
        return false;
    }

    size_t pos = colon + 1;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
    {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '"')
    {
        return false;
    }

    ++pos;
    std::string parsed;
    while (pos < json.size())
    {
        char ch = json[pos++];
        if (ch == '"')
        {
            *value = parsed;
            return true;
        }
        if (ch == '\\' && pos < json.size())
        {
            char escaped = json[pos++];
            switch (escaped)
            {
            case '"':
            case '\\':
            case '/':
                parsed += escaped;
                break;
            case 'n':
                parsed += '\n';
                break;
            case 'r':
                parsed += '\r';
                break;
            case 't':
                parsed += '\t';
                break;
            default:
                parsed += escaped;
                break;
            }
        }
        else
        {
            parsed += ch;
        }
    }

    return false;
}

std::string jsonError(const std::string &message)
{
    return "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
}

std::string httpResponse(const std::string &status,
                         const std::string &contentType,
                         const std::string &body)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string jsonResponse(const std::string &status, const std::string &body)
{
    return httpResponse(status, "application/json; charset=utf-8", body);
}
