#include "ChatServer.h"

extern map<string, UserInfo> gUserMap;
extern map<int, LicenseInstance> gLicenseMap;
extern vector <ConnectionInfo> gConnectionList;

void ChatServer::Start() {
    server_.Init();
    server_.Start();
}

int ChatServer::GetIntFromDOM(Document* doc, string key) {
    if (!doc->HasMember(key)) {
        cout << "Parse Error: " << key << " field does not exist" << endl;
        return -1;
    }

    return (*doc)[key].GetInt();
}

string ChatServer::GetStrFromDOM(Document* doc, string key) {
    if (!doc->HasMember(key)) {
        cout << "Parse Error: " << key << " field does not exist" << endl;
        return "";
    }

    return (*doc)[key].GetString();
}

string ChatServer::GenerateAccessToken(UserInfo* pInfo) {
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

string ChatServer::GenerateLicenseAccessToken(LicenseInstance* pLicense) {
    // Get the current time
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(pLicense->accessTime.time_since_epoch()).count();

    // Combine the timestamp, user ID, and username
    std::stringstream input;
    input << "MMORPG" << timestamp << pLicense->licenseDefinition.gameInfo.productName << pLicense->licenseDefinition.variationType;
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

string ChatServer::doSignin(const evpp::TCPConnPtr& conn, string userid, string password) {
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

            if (pInfo->inUse == 1) {
                doc.AddMember("success", 0, allocator);
                doc.AddMember("msg", "Error: Another user has already logged in.", allocator);
            }

            else {
                if (pInfo->accessToken.length() == 0 || (chrono::system_clock::now() - pInfo->accessTime) > std::chrono::minutes(30)) {
                    pInfo->accessTime = chrono::system_clock::now();
                    pInfo->accessToken = GenerateAccessToken(pInfo);
                }

                pInfo->conn = conn;

                doc.AddMember("success", 1, allocator);
                doc.AddMember("accessToken", pInfo->accessToken, allocator);

                Value licenses(kArrayType);

                for (int i = 0; i < pInfo->licenses.size(); i++) {
                    LicenseInstance* pLicense = (LicenseInstance*)&gLicenseMap[pInfo->licenses[i]];
                    Value item(kObjectType);

                    if (pLicense->accessToken.length() == 0 || (chrono::system_clock::now() - pLicense->createdTime) > std::chrono::minutes(30)) {
                        pLicense->createdTime = chrono::system_clock::now();
                        pLicense->accessToken = GenerateLicenseAccessToken(pLicense);
                    }

                    item.AddMember("ID", pInfo->licenses[i], allocator);
                    item.AddMember("licenseDefinitionID", pLicense->licenseDefinition.id, allocator);
                    item.AddMember("gameID", pLicense->licenseDefinition.gameInfo.gameid, allocator);
                    item.AddMember("productName", pLicense->licenseDefinition.gameInfo.productName, allocator);
                    item.AddMember("type", pLicense->licenseDefinition.variationType, allocator);
                    item.AddMember("accessToken", pLicense->accessToken, allocator);

                    licenses.PushBack(item, allocator);
                }

                pInfo->inUse = 1;

                doc.AddMember("licenses", licenses, allocator);
            }
        }
    }

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}
string ChatServer::GenerateLicenseUseMessage(UserInfo* pInfo, int licenseid, int status) {
    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();
    doc.AddMember("action", "command", allocator);
    doc.AddMember("licenseid", licenseid, allocator);
    doc.AddMember("type", 0, allocator);

    string msg = (status == 1 ? "yes" : "no");
    doc.AddMember("msg", msg, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

string ChatServer::doConnectLicense(const evpp::TCPConnPtr& conn, string userid, int licenseid, string token) {

    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("action", "connectLicense", allocator);

    if (licenseid < 1 || token.length() < 3) {
        doc.AddMember("success", 0, allocator);
        doc.AddMember("msg", "Error: Please input the AccessToken correctly.", allocator);
    }
    else if (licenseid >= gLicenseMap.size()) {
        doc.AddMember("success", 0, allocator);
        doc.AddMember("msg", "Error: Large license id is delived.", allocator);
    }
    else {
        int find = 0;
        LicenseInstance* pInfo = (LicenseInstance*)&gLicenseMap[licenseid];

        if (pInfo && pInfo->accessToken == token) {
            int success = 0;

            if (pInfo->inUse == 1) {
                doc.AddMember("success", 0, allocator);
                doc.AddMember("msg", "Error: License is in use.", allocator);
            }

            else if (pInfo->accessTime.time_since_epoch() == std::chrono::seconds(0)) {
                pInfo->accessTime = chrono::system_clock::now();
                success = 1;
            }
            else if ((chrono::system_clock::now() - pInfo->accessTime) < std::chrono::minutes(30)) {
                success = 1;
            }

            else {
                pInfo->accessToken = GenerateLicenseAccessToken(pInfo);
                doc.AddMember("success", 0, allocator);
                doc.AddMember("msg", "Error: AccessToken is expired.", allocator);
            }

            if (success == 1) {

                auto it = gUserMap.find(userid);
                if (it == gUserMap.end()) {
                    doc.AddMember("success", 0, allocator);
                    doc.AddMember("msg", "Error: Failed to find userid in the user map.", allocator);
                }
                else {
                    doc.AddMember("success", 1, allocator);
                    UserInfo* pUserInfo = (UserInfo*)&it->second;
                    string res = GenerateLicenseUseMessage(pUserInfo, licenseid, 1);
                    codec_.Send(pUserInfo->conn, res);

                    pInfo->inUse = 1;
                    gConnectionList.push_back({ conn, userid, licenseid });
                }
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

struct ConnectionInfoComparator {
    bool operator()(const ConnectionInfo& ci, const evpp::TCPConnPtr& conn) const {
        return ci.conn == conn;
    }
};

BOOL ChatServer::RemoveClinet(const evpp::TCPConnPtr& conn)
{
    for (auto& userPair : gUserMap) {
        if (userPair.second.conn == conn) {
            userPair.second.inUse = 0;
            return true;
        }
    }
    return false;
}

BOOL ChatServer::NotifyClientUpdateLicenseStatus(const evpp::TCPConnPtr& conn)
{
    auto it = std::find_if(gConnectionList.begin(), gConnectionList.end(), [&](const ConnectionInfo& ci) {
        return ci.conn == std::static_pointer_cast<evpp::TCPConn>(conn);
    });

    if (it != gConnectionList.end()) {
        auto userMapit = gUserMap.find(it->userid);
        if (userMapit != gUserMap.end()) {
            UserInfo* pUserInfo = (UserInfo*)&userMapit->second;
            string res = GenerateLicenseUseMessage(pUserInfo, it->licenseid, 0);
            codec_.Send(pUserInfo->conn, res);
        }
        auto licenseMapit = gLicenseMap.find(it->licenseid);
        if (licenseMapit != gLicenseMap.end()) {
            LicenseInstance* pLicenseInfo = (LicenseInstance*)&licenseMapit->second;
            pLicenseInfo->inUse = 0;
        }
        gConnectionList.erase(it);

        return true;
    }

    return false;
}

void ChatServer::OnConnection(const evpp::TCPConnPtr& conn) {
    LOG_INFO << conn->AddrToString() << " is " << (conn->IsConnected() ? "UP" : "DOWN");
    if (conn->IsConnected()) {
        connections_.insert(conn);
    }
    else {
        connections_.erase(conn);
        if (RemoveClinet(conn) == false) { // Need to check connection is related to license instance.
            NotifyClientUpdateLicenseStatus(conn);
        }
    }
}

void ChatServer::OnStringMessage(const evpp::TCPConnPtr& conn,
    const std::string& message) {

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

    string action = GetStrFromDOM(&document, "action");
    if (action == "signin") {
        string userid = GetStrFromDOM(&document, "userid");
        string userpass = GetStrFromDOM(&document, "password");
        string ret = doSignin(conn, userid, userpass);

        if (ret.length() > 2)
            codec_.Send(conn, ret);
        else
            codec_.Send(conn, "{}");
    }
    else if (action == "connectLicense") {
        int licenseid = GetIntFromDOM(&document, "licenseid");
        string userid = GetStrFromDOM(&document, "userid");
        string token = GetStrFromDOM(&document, "accessToken");
        string ret = doConnectLicense(conn, userid, licenseid, token);

        if (ret.length() > 2)
            codec_.Send(conn, ret);
        else
            codec_.Send(conn, "{}");
    }
}