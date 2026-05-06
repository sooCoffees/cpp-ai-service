#include <UserStore.h>

#include <HttpCodec.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <sys/stat.h>

namespace
{
const int kDefaultSessionTtlSeconds = 24 * 60 * 60;
const int kDefaultRegistrationLimitPerMinute = 5;

std::string getenvString(const char *name)
{
    const char *value = std::getenv(name);
    if (value == nullptr)
    {
        return "";
    }
    return value;
}

int getenvPositiveInt(const char *name, int fallback)
{
    const char *value = std::getenv(name);
    if (value == nullptr || *value == '\0')
    {
        return fallback;
    }

    const int parsed = std::atoi(value);
    if (parsed <= 0)
    {
        return fallback;
    }
    return parsed;
}

bool ensureParentDirectory(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
    {
        return true;
    }

    const std::string dir = path.substr(0, slash);
    if (dir.empty())
    {
        return true;
    }

    if (::mkdir(dir.c_str(), 0700) == 0)
    {
        return true;
    }

    struct stat st;
    return ::stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

}

UserStore::UserStore(const std::string &path)
    : path_(path)
    , inviteCode_(getenvString("CPP_AI_REGISTRATION_INVITE_CODE"))
    , sessionTtlSeconds_(getenvPositiveInt("CPP_AI_SESSION_TTL_SECONDS", kDefaultSessionTtlSeconds))
    , registrationLimitPerMinute_(getenvPositiveInt("CPP_AI_REGISTRATION_LIMIT_PER_MINUTE", kDefaultRegistrationLimitPerMinute))
{
}

bool UserStore::load(std::string *error)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureParentDirectory(path_))
    {
        if (error != nullptr)
        {
            *error = "failed to create user data directory";
        }
        return false;
    }

    std::ifstream input(path_.c_str());
    if (!input.good())
    {
        return true;
    }

    std::string line;
    while (std::getline(input, line))
    {
        if (trim(line).empty())
        {
            continue;
        }

        StoredUser user;
        if (!extractJsonStringField(line, "username", &user.username) ||
            !extractJsonStringField(line, "role", &user.role) ||
            !extractJsonStringField(line, "salt", &user.salt) ||
            !extractJsonStringField(line, "password_hash", &user.passwordHash))
        {
            if (error != nullptr)
            {
                *error = "failed to parse user data file";
            }
            return false;
        }

        users_[user.username] = user;
    }

    return true;
}

AuthResult UserStore::registerUser(const std::string &username,
                                   const std::string &password,
                                   const std::string &inviteCode,
                                   const std::string &clientId)
{
    AuthResult result;
    result.ok = false;

    std::string validationError;
    if (!validateUsername(username, &validationError) || !validatePassword(password, &validationError))
    {
        result.error = validationError;
        return result;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (isRegistrationLimitedLocked(clientId))
    {
        result.error = "too many registration attempts";
        return result;
    }

    if (!inviteCode_.empty() && inviteCode != inviteCode_)
    {
        result.error = "invalid invite code";
        return result;
    }

    if (users_.find(username) != users_.end())
    {
        result.error = "username already exists";
        return result;
    }

    StoredUser user;
    user.username = username;
    user.role = "user";
    user.salt = randomHex(16);
    user.passwordHash = demoHashPassword(user.salt, password);

    std::string writeError;
    if (!appendUserLocked(user, &writeError))
    {
        result.error = writeError;
        return result;
    }

    users_[user.username] = user;
    result.ok = true;
    result.user.username = user.username;
    result.user.role = user.role;
    return result;
}

AuthResult UserStore::login(const std::string &username, const std::string &password)
{
    AuthResult result;
    result.ok = false;

    std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::string, StoredUser>::const_iterator found = users_.find(username);
    if (found == users_.end())
    {
        result.error = "invalid username or password";
        return result;
    }

    const std::string expected = demoHashPassword(found->second.salt, password);
    if (expected != found->second.passwordHash)
    {
        result.error = "invalid username or password";
        return result;
    }

    result.ok = true;
    result.user.username = found->second.username;
    result.user.role = found->second.role;
    result.session = createSessionLocked(found->second);
    return result;
}

AuthResult UserStore::authenticateBearerToken(const std::string &authorizationHeader)
{
    AuthResult result;
    result.ok = false;

    const std::string prefix = "Bearer ";
    if (authorizationHeader.compare(0, prefix.size(), prefix) != 0)
    {
        result.error = "missing bearer token";
        return result;
    }

    const std::string token = trim(authorizationHeader.substr(prefix.size()));
    if (token.empty())
    {
        result.error = "missing bearer token";
        return result;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, SessionInfo>::iterator found = sessions_.find(token);
    if (found == sessions_.end())
    {
        result.error = "invalid session token";
        return result;
    }

    if (std::chrono::system_clock::now() >= found->second.expiresAt)
    {
        sessions_.erase(found);
        result.error = "session token expired";
        return result;
    }

    result.ok = true;
    result.user = found->second.user;
    result.session = found->second;
    return result;
}

std::string UserStore::configJson() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream body;
    body << "{"
         << "\"user_count\":" << users_.size() << ","
         << "\"session_count\":" << sessions_.size() << ","
         << "\"invite_required\":" << (inviteCode_.empty() ? "false" : "true") << ","
         << "\"session_ttl_seconds\":" << sessionTtlSeconds_ << ","
         << "\"registration_limit_per_minute\":" << registrationLimitPerMinute_ << ","
         << "\"password_hash\":\"demo_only_fnv1a\""
         << "}";
    return body.str();
}

bool UserStore::isRegistrationLimitedLocked(const std::string &clientId)
{
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const std::chrono::steady_clock::time_point windowStart = now - std::chrono::minutes(1);
    std::vector<std::chrono::steady_clock::time_point> &attempts = registrationAttempts_[clientId];

    std::vector<std::chrono::steady_clock::time_point> kept;
    for (size_t i = 0; i < attempts.size(); ++i)
    {
        if (attempts[i] >= windowStart)
        {
            kept.push_back(attempts[i]);
        }
    }
    attempts.swap(kept);

    if (attempts.size() >= static_cast<size_t>(registrationLimitPerMinute_))
    {
        return true;
    }

    attempts.push_back(now);
    return false;
}

bool UserStore::appendUserLocked(const StoredUser &user, std::string *error) const
{
    if (!ensureParentDirectory(path_))
    {
        if (error != nullptr)
        {
            *error = "failed to create user data directory";
        }
        return false;
    }

    std::ofstream output(path_.c_str(), std::ios::out | std::ios::app);
    if (!output.good())
    {
        if (error != nullptr)
        {
            *error = "failed to open user data file";
        }
        return false;
    }

    output << "{"
           << "\"username\":\"" << jsonEscape(user.username) << "\","
           << "\"role\":\"" << jsonEscape(user.role) << "\","
           << "\"salt\":\"" << jsonEscape(user.salt) << "\","
           << "\"password_hash\":\"" << jsonEscape(user.passwordHash) << "\""
           << "}\n";
    output.flush();
    if (!output.good())
    {
        if (error != nullptr)
        {
            *error = "failed to write user data file";
        }
        return false;
    }

    return true;
}

SessionInfo UserStore::createSessionLocked(const StoredUser &user)
{
    SessionInfo session;
    session.token = randomHex(32);
    session.user.username = user.username;
    session.user.role = user.role;
    session.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(sessionTtlSeconds_);
    sessions_[session.token] = session;
    return session;
}

bool UserStore::validateUsername(const std::string &username, std::string *error)
{
    if (username.size() < 3 || username.size() > 32)
    {
        if (error != nullptr)
        {
            *error = "username must be 3 to 32 characters";
        }
        return false;
    }

    for (size_t i = 0; i < username.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(username[i]);
        if (!std::isalnum(ch) && ch != '_' && ch != '-')
        {
            if (error != nullptr)
            {
                *error = "username can only contain letters, numbers, underscores, and hyphens";
            }
            return false;
        }
    }

    return true;
}

bool UserStore::validatePassword(const std::string &password, std::string *error)
{
    if (password.size() < 8 || password.size() > 128)
    {
        if (error != nullptr)
        {
            *error = "password must be 8 to 128 characters";
        }
        return false;
    }
    return true;
}

std::string UserStore::demoHashPassword(const std::string &salt, const std::string &password)
{
    const std::string input = salt + ":" + password;
    unsigned long long hash = 1469598103934665603ULL;
    for (size_t i = 0; i < input.size(); ++i)
    {
        hash ^= static_cast<unsigned char>(input[i]);
        hash *= 1099511628211ULL;
    }

    for (int round = 0; round < 4096; ++round)
    {
        hash ^= static_cast<unsigned long long>(round);
        hash *= 1099511628211ULL;
        for (size_t i = 0; i < salt.size(); ++i)
        {
            hash ^= static_cast<unsigned char>(salt[i]);
            hash *= 1099511628211ULL;
        }
    }

    return toHex(hash);
}

std::string UserStore::randomHex(size_t bytes)
{
    std::random_device device;
    std::ostringstream body;
    for (size_t i = 0; i < bytes; ++i)
    {
        const unsigned int value = device() & 0xff;
        body << std::hex << std::setw(2) << std::setfill('0') << value;
    }
    return body.str();
}

std::string UserStore::toHex(unsigned long long value)
{
    std::ostringstream body;
    body << std::hex << std::setw(16) << std::setfill('0') << value;
    return body.str();
}
