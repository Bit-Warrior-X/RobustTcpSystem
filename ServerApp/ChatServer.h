#pragma once

#include "codec.h"

#include <evpp/event_loop.h>
#include <evpp/tcp_server.h>

#include <mutex>
#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <map>
#include <unordered_map>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <openssl/sha.h>
#include <openssl/evp.h>

class TCPConn;

using namespace std;
using namespace rapidjson;

typedef struct UserInfo {
    int id;
    string userid;
    string username;
    string password;
    vector<int> licenses;
    string accessToken;
    chrono::system_clock::time_point accessTime;
    evpp::TCPConnPtr conn;
    int inUse;
} UserInfo;


struct GameInfo
{
    int gameid;         // 0: Game1, 1: Game2, 2: Game3
    string productName; // GameA, GameB
};

enum LicenseVariationType
{
    ELicenseVariationType_Lite,
    ELicenseVariationType_Regular,
};

struct LicenseDefinition
{
    int id;
    GameInfo gameInfo;
    LicenseVariationType variationType; // 0: Lite, 1: Regular
};

struct LicenseInstance
{
    int id;
    LicenseDefinition licenseDefinition;
    string userId;
    string accessToken;
    chrono::system_clock::time_point createdTime;
    chrono::system_clock::time_point accessTime;
    bool inUse;
};

typedef struct ConnectionInfo {
    evpp::TCPConnPtr conn;
    string userid;
    int licenseid;
} ConnectionInfo;

class ChatServer {

private:
    typedef std::set<evpp::TCPConnPtr> ConnectionList;
    evpp::TCPServer server_;
    LengthHeaderCodec codec_;
    ConnectionList connections_;

public:
    ChatServer(evpp::EventLoop* loop,
        const std::string& addr)
        : server_(loop, addr, "ChatServer", 0),
        codec_(std::bind(&ChatServer::OnStringMessage, this, std::placeholders::_1, std::placeholders::_2)) {
        server_.SetConnectionCallback(
            std::bind(&ChatServer::OnConnection, this, std::placeholders::_1));
        server_.SetMessageCallback(
            std::bind(&LengthHeaderCodec::OnMessage, &codec_, std::placeholders::_1, std::placeholders::_2));
    }

    void Start();

private:

    int GetIntFromDOM(Document* doc, string key);
    string GetStrFromDOM(Document* doc, string key);

    string GenerateAccessToken(UserInfo* pInfo);
    string GenerateLicenseAccessToken(LicenseInstance* pLicense);

    void OnConnection(const evpp::TCPConnPtr& conn);
    void OnStringMessage(const evpp::TCPConnPtr& conn, const std::string& message);

    string doSignin(const evpp::TCPConnPtr& conn, string userid, string password);
    string doConnectLicense(const evpp::TCPConnPtr& conn, string userid, int licenseid, string token);

    string GenerateLicenseUseMessage(UserInfo* pInfo, int licenseid, int status);
    BOOL NotifyClientUpdateLicenseStatus(const evpp::TCPConnPtr& conn);
    BOOL RemoveClinet(const evpp::TCPConnPtr& conn);
};


