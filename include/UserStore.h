#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct UserProfile
{
    std::string username;
    std::string role;
};

struct SessionInfo
{
    std::string token;
    UserProfile user;
    std::chrono::system_clock::time_point expiresAt;
};

struct AuthResult
{
    bool ok;
    std::string error;
    UserProfile user;
    SessionInfo session;
};

class UserStore
{
public:
    explicit UserStore(const std::string &path = "data/users.jsonl");

    bool load(std::string *error);

    AuthResult registerUser(const std::string &username,
                            const std::string &password,
                            const std::string &inviteCode,
                            const std::string &clientId);

    AuthResult login(const std::string &username, const std::string &password);
    AuthResult authenticateBearerToken(const std::string &authorizationHeader);
    std::string configJson() const;

private:
    struct StoredUser
    {
        std::string username;
        std::string role;
        std::string salt;
        std::string passwordHash;
    };

    bool isRegistrationLimitedLocked(const std::string &clientId);
    bool appendUserLocked(const StoredUser &user, std::string *error) const;
    SessionInfo createSessionLocked(const StoredUser &user);

    static bool validateUsername(const std::string &username, std::string *error);
    static bool validatePassword(const std::string &password, std::string *error);
    static std::string demoHashPassword(const std::string &salt, const std::string &password);
    static std::string randomHex(size_t bytes);
    static std::string toHex(unsigned long long value);

    std::string path_;
    std::string inviteCode_;
    int sessionTtlSeconds_;
    int registrationLimitPerMinute_;

    mutable std::mutex mutex_;
    std::map<std::string, StoredUser> users_;
    std::map<std::string, SessionInfo> sessions_;
    std::map<std::string, std::vector<std::chrono::steady_clock::time_point> > registrationAttempts_;
};
