#include "ChatClient.h"


ChatClient::~ChatClient()
{
    std::lock_guard<std::mutex> lock(mutex_);
    client_.Disconnect();
}

void ChatClient::Connect(std::string id, std::string password)
{
    userid = id;
    userpass = password;

    if (connection_) {
        codec_.Send(connection_, GenerateConnectionRequestMessage());
    } else 
	    client_.Connect();
}

void ChatClient::Disconnect() 
{
    client_.Disconnect();
}

void ChatClient::Write(const evpp::Slice& message) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
        codec_.Send(connection_, message);
    }
}

string ChatClient::GetStrFromDOM(Document* doc, string key) 
{
    if (!doc->HasMember(key)) {
        cout << "Parse Error: " << key << " field does not exist" << endl;
        return "";
    }

    return (*doc)[key].GetString();
}

int ChatClient::GetIntFromDOM(Document* doc, string key) 
{
    if (!doc->HasMember(key)) {
        cout << "Parse Error:" << key << " field does not exist" << endl;
        return -1;
    }

    return (*doc)[key].GetInt();
}

void ChatClient::OnConnection(const evpp::TCPConnPtr& conn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn->IsConnected()) {
        connection_ = conn;
        codec_.Send(connection_, GenerateConnectionRequestMessage());
    }
    else {
        if (conn->IsDisconnected() == true && conn->fd() == -1) {
            if (login_message_callback_) {
                login_message_callback_("Connection failed to server. Please start server", RESPONSE_SERVER_ERR);
            }
        }
        connection_.reset();
        connection_ = NULL;
    }
}

void ChatClient::OnStringMessage(const evpp::TCPConnPtr& con,
    const std::string& message) {
    Document document;
    document.Parse(message.c_str());

    if (!document.IsObject()) {
        if (login_message_callback_) {
            login_message_callback_("Parse reponse error", RESPONSE_SERVER_ERR);
        }
        return;
    }
    if (!document.HasMember("action")) {
        if (login_message_callback_) {
            login_message_callback_("Parse Error: action field does not exist", RESPONSE_SERVER_ERR);
        }
        return;
    }

    string action = GetStrFromDOM(&document, "action");
    if (action == "signin") {
        ParseSignInMessage(&document);
    }
    else if (action == "command") {
        ParseCommandMessage(&document);
    }
}

void ChatClient::ParseSignInMessage(Document* document)
{
    int success = GetIntFromDOM(document, "success");

    if (success == RC_OK) {
        string token = GetStrFromDOM(document, "accessToken");

        this->accessToken = token;
        this->accessTime = chrono::system_clock::now();

        rapidjson::Value* jnode = &((*document)["licenses"]);
        const Value& licenses = *jnode;

        licensesInfo.clear();

        for (rapidjson::SizeType i = 0; i < licenses.Size(); ++i) {
            LicenseInstance new_license;
            const Value& id = licenses[i]["ID"];
            const Value& licenseDefinitionID = licenses[i]["licenseDefinitionID"];
            const Value& gameID = licenses[i]["gameID"];
            const Value& productName = licenses[i]["productName"];
            const Value& type = licenses[i]["type"];
            const Value& licenseAccessToken = licenses[i]["accessToken"];

            new_license.id = id.GetInt();
            new_license.licenseDefinition.id = licenseDefinitionID.GetInt();
            new_license.licenseDefinition.gameInfo.gameid = gameID.GetInt();
            new_license.licenseDefinition.gameInfo.productName = productName.GetString();
            new_license.licenseDefinition.variationType = (LicenseVariationType)type.GetInt();
            new_license.accessToken = licenseAccessToken.GetString();
            new_license.accessTime = chrono::system_clock::now();
            new_license.inUse = 0;

            licensesInfo.push_back(new_license);
        }
        if (login_message_callback_) {
            login_message_callback_("Success", RESPONSE_RETURN_SUC);
        }
    }
    else if (success == RC_FAILED) {
        string msg = GetStrFromDOM(document, "msg");
        if (login_message_callback_) {
            login_message_callback_(msg, RESPONSE_RETURN_ERR);
        }
        Disconnect();
    }
    else {
        if (login_message_callback_) {
            login_message_callback_("Parse Error: success field does not exist", RESPONSE_SERVER_ERR);
        }
    }
}

void ChatClient::ParseCommandMessage(Document* document)
{
    int type = GetIntFromDOM(document, "type");

    switch (type) {
    case 0: {// Update use statue of license 
        string msg = GetStrFromDOM(document, "msg");
        int licenseid = GetIntFromDOM(document, "licenseid");
        int status = 0;

        msg == "yes" ? status = 1 : status = 0;

        for (int i = 0; i < licensesInfo.size(); i++) {
            if (licensesInfo.at(i).id == licenseid) {
                licensesInfo.at(i).inUse = status;
                break;
            }
        }

        if (main_message_callback_)
            main_message_callback_(licenseid, type, msg);
        break;
    }
    default:
        break;
    }
}

void ChatClient::SetLoginMessageCallback(const std::function<void(const std::string&, int status)>& callback)
{
    login_message_callback_ = callback;
}

void ChatClient::SetMainMessageCallback(const std::function<void(int licenseid, int type, string& msg)>& callback)
{
    main_message_callback_ = callback;
}

string ChatClient::GetHostAddress()
{
    return hostAddress;
}

string ChatClient::GetUserID()
{
    return userid;
}

vector <LicenseInstance> ChatClient::GetLicenseList()
{
    return licensesInfo;
}

LicenseInstance& ChatClient::GetLicenseInstance(int licenseID) {
    for (const auto& license : licensesInfo) {
        if (license.id == licenseID) {
            return const_cast<LicenseInstance&>(license); // Remove constness to match return type
        }
    }
    throw std::out_of_range("License ID not found");
}

int ChatClient::GetNetworkStatus() {
    return connection_->status();
}

string ChatClient::GenerateConnectionRequestMessage() {
    StringBuffer s;

    Writer<StringBuffer> writer(s);
    writer.StartObject();
    writer.Key("action");       writer.String("signin");
    writer.Key("userid");       writer.String(userid);
    writer.Key("password");       writer.String(userpass);
    writer.EndObject();

    return s.GetString();
}