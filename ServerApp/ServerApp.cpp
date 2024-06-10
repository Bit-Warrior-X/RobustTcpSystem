// ServerApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
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
} UserInfo;

typedef struct LicenseInfo {
    int id;
    int type;   // 0: lite, 1: regular
    int use;    // 0: Unused, 1: Used
    string userid;
    string productName;
    string accessToken;
    chrono::system_clock::time_point createdTime;
    chrono::system_clock::time_point accessTime;
} LicenseInfo;

map<string, UserInfo> gUserMap;
map<int, LicenseInfo> gLicenseMap;

class ChatServer {
public:
    ChatServer(evpp::EventLoop* loop,
        const std::string& addr)
        : server_(loop, addr, "ChatServer", 0),
        codec_(std::bind(&ChatServer::OnStringMessage, this, std::placeholders::_1, std::placeholders::_2)) {
        server_.SetConnectionCallback(
            std::bind(&ChatServer::OnConnection, this, std::placeholders::_1));
        server_.SetMessageCallback(
            std::bind(&LengthHeaderCodec::OnMessage, &codec_, std::placeholders::_1, std::placeholders::_2));

        LicenseInfo license1 = { 1, 0, 0, "david", "Product1", "", chrono::system_clock::time_point() };
        LicenseInfo license2 = { 2, 0, 0, "david", "Product2", "", chrono::system_clock::time_point() };

        gLicenseMap.insert(std::make_pair(license1.id, license1));
        gLicenseMap.insert(std::make_pair(license2.id, license2));

        // Add some sample user information
        UserInfo user1 = { 1, "john_doe", "John Doe", "abc123", vector<int>(), "", chrono::system_clock::time_point() };
        UserInfo user2 = { 2, "jane_smith", "Jane Smith", "def456", vector<int>(), "", chrono::system_clock::time_point() };
        UserInfo user3 = { 3, "bob_johnson", "Bob Johnson", "ghi789", vector<int>(), "", chrono::system_clock::time_point() };
        UserInfo user4 = { 4, "david", "David", "david123", {1,2}, "", chrono::system_clock::time_point() };

        gUserMap.insert(std::make_pair(user1.userid, user1));
        gUserMap.insert(std::make_pair(user2.userid, user2));
        gUserMap.insert(std::make_pair(user3.userid, user3));
        gUserMap.insert(std::make_pair(user4.userid, user4));
    }

    void Start() {
        server_.Init();
        server_.Start();
    }

private:
    int getIntFromDOM(Document* doc, string key) {
        if (!doc->HasMember(key)) {
            cout << "Parse Error: " << key << " field does not exist" << endl;
            return -1;
        }

        return (*doc)[key].GetInt();
    }

    string getStrFromDOM(Document* doc, string key) {
        if (!doc->HasMember(key)) {
            cout << "Parse Error: " << key << " field does not exist" << endl;
            return "";
        }

        return (*doc)[key].GetString();
    }

    string generateAccessToken(UserInfo* pInfo) {
        // Get the current time
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(pInfo->accessTime.time_since_epoch()).count();

        // Combine the timestamp, user ID, and username
        std::stringstream input;
        input << "MMORPG" << timestamp << pInfo->userid << pInfo->username;
        std::string inputString = input.str();

        // Hash the input using SHA-256
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, inputString.c_str(), inputString.length());
        SHA256_Final(hash, &sha256);

        std::string tokenString;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            char buffer[3] = "";
            snprintf(buffer, sizeof(buffer), "%02x", hash[i]);
            tokenString += buffer;
        }

        return tokenString;
    }

    string generateLicenseAccessToken(LicenseInfo* pLicense) {
        // Get the current time
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(pLicense->accessTime.time_since_epoch()).count();

        // Combine the timestamp, user ID, and username
        std::stringstream input;
        input << "MMORPG" << timestamp << pLicense->userid << pLicense->productName << pLicense->type;
        std::string inputString = input.str();

        // Hash the input using SHA-256
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, inputString.c_str(), inputString.length());
        SHA256_Final(hash, &sha256);

        std::string tokenString;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            char buffer[3] = "";
            snprintf(buffer, sizeof(buffer), "%02x", hash[i]);
            tokenString += buffer;
        }

        return tokenString;
    }

    void OnConnection(const evpp::TCPConnPtr& conn) {
        LOG_INFO << conn->AddrToString() << " is " << (conn->IsConnected() ? "UP" : "DOWN");
        if (conn->IsConnected()) {
            connections_.insert(conn);
        }
        else {
            connections_.erase(conn);
        }
    }

    string doSignin(string userid, string password) {
        Document doc;

        doc.SetObject();

        Document::AllocatorType& allocator = doc.GetAllocator();

        doc.AddMember("action", "signin", allocator);

        if (userid.length() < 3 || password.length() < 3) {
            doc.AddMember("success", 0, allocator);
            doc.AddMember("msg", "Error: Please input the userid or password correctly.", allocator);
        }
        else {
            auto it = gUserMap.find(userid);
            if (it == gUserMap.end()) {
                doc.AddMember("success", 0, allocator);
                doc.AddMember("msg", "Error: Please input the userid correctly.", allocator);
            }
            else {
                UserInfo* pInfo = (UserInfo*)&it->second;

                if (pInfo->accessToken.length() == 0 || (chrono::system_clock::now() - pInfo->accessTime) > std::chrono::minutes(30)) {
                    pInfo->accessTime = chrono::system_clock::now();
                    pInfo->accessToken = generateAccessToken(pInfo);
                }

                doc.AddMember("success", 1, allocator);
                doc.AddMember("accessToken", pInfo->accessToken, allocator);

                Value licenses(kArrayType);

                for (int i = 0; i < pInfo->licenses.size(); i++) {
                    LicenseInfo* pLicense = (LicenseInfo*)&gLicenseMap[pInfo->licenses[i]];
                    Value item(kObjectType);

                    if (pLicense->accessToken.length() == 0 || (chrono::system_clock::now() - pLicense->createdTime) > std::chrono::minutes(30)) {
                        pLicense->createdTime = chrono::system_clock::now();
                        pLicense->accessToken = generateLicenseAccessToken(pLicense);
                    }

                    item.AddMember("id", pInfo->licenses[i], allocator);
                    item.AddMember("type", pLicense->type, allocator);
                    item.AddMember("productName", pLicense->productName, allocator);
                    item.AddMember("accessToken", pLicense->accessToken, allocator);

                    licenses.PushBack(item, allocator);
                }

                doc.AddMember("licenses", licenses, allocator);
            }
        }

        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);

        return buffer.GetString();
    }

    string doConnectLicense(int id, string token) {
        Document doc;

        doc.SetObject();

        Document::AllocatorType& allocator = doc.GetAllocator();

        doc.AddMember("action", "connectLicense", allocator);

        if (id < 1 || token.length() < 3) {
            doc.AddMember("success", 0, allocator);
            doc.AddMember("msg", "Error: Please input the AccessToken correctly.", allocator);
        }
        else {
            int find = 0;
            LicenseInfo* pInfo = (LicenseInfo*)&gLicenseMap[id];

            if (pInfo && pInfo->accessToken == token) {
                if (pInfo->accessTime.time_since_epoch() == std::chrono::seconds(0)) {
                    pInfo->accessTime = chrono::system_clock::now();
                    doc.AddMember("success", 1, allocator);
                }
                else if ((chrono::system_clock::now() - pInfo->accessTime) < std::chrono::minutes(30)) {
                    doc.AddMember("success", 1, allocator);
                }
                else {
                    pInfo->accessToken = generateLicenseAccessToken(pInfo);

                    doc.AddMember("success", 0, allocator);
                    doc.AddMember("msg", "Error: AccessToken is expired.", allocator);
                }
            }
            else {
                doc.AddMember("success", 0, allocator);
                doc.AddMember("msg", "Error: Please input the AccessToken correctly.", allocator);
            }
        }

        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);

        return buffer.GetString();
    }

    void OnStringMessage(const evpp::TCPConnPtr& conn,
        const std::string& message) {
        /*
        for (ConnectionList::iterator it = connections_.begin();
             it != connections_.end();
             ++it) {
            codec_.Send(*it, message);
        }
        */

        Document document;
        document.Parse(message.c_str());

        if (!document.IsObject()) {
            cout << "Parse Error" << endl;
            return;
        }
        if (!document.HasMember("action")) {
            cout << "Parse Error: action field does not exist" << endl;
            return;
        }

        string action = getStrFromDOM(&document, "action");
        if (action == "signin") {
            string userid = getStrFromDOM(&document, "userid");
            string userpass = getStrFromDOM(&document, "password");
            string ret = doSignin(userid, userpass);

            if (ret.length() > 2)
                codec_.Send(conn, ret);
            else
                codec_.Send(conn, "{}");
        }
        else if (action == "connectLicense") {
            int id = getIntFromDOM(&document, "id");
            string token = getStrFromDOM(&document, "accessToken");
            string ret = doConnectLicense(id, token);

            if (ret.length() > 2)
                codec_.Send(conn, ret);
            else
                codec_.Send(conn, "{}");
        }
    }

    typedef std::set<evpp::TCPConnPtr> ConnectionList;
    evpp::TCPServer server_;
    LengthHeaderCodec codec_;
    ConnectionList connections_;
};

int main(int argc, char* argv[]) {
    evpp::EventLoop loop;
    std::string addr = std::string("0.0.0.0:9099");
    ChatServer server(&loop, addr);
    server.Start();
    loop.Run();
    return 0;
}

