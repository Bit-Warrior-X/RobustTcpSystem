// ClientApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "codec.h"

#include <evpp/event_loop.h>
#include <evpp/event_loop_thread.h>
#include <evpp/tcp_client.h>

#include <mutex>
#include <iostream>
#include <string>
#include <cstdio>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

typedef struct LicenseInfo {
    int id;
    int type;   // 0: lite, 1: regular
    string productName;
    string accessToken;
    chrono::system_clock::time_point createdTime;
    chrono::system_clock::time_point accessTime;
} ClientLicenseInfo;

class ChatClient {
public:
    ChatClient(evpp::EventLoop* loop, const std::string& serverAddr)
        : client_(loop, serverAddr, "ChatClient"),
        codec_(std::bind(&ChatClient::OnStringMessage, this, std::placeholders::_1, std::placeholders::_2)) {
        client_.SetConnectionCallback(
            std::bind(&ChatClient::OnConnection, this, std::placeholders::_1));
        client_.SetMessageCallback(
            std::bind(&LengthHeaderCodec::OnMessage, &codec_, std::placeholders::_1, std::placeholders::_2));
    }

    void Connect() {
        client_.Connect();
    }

    void Disconnect() {
        client_.Disconnect();
    }

    void Write(const evpp::Slice& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connection_) {
            codec_.Send(connection_, message);
        }
    }

private:
    string getStrFromDOM(Document* doc, string key) {
        if (!doc->HasMember(key)) {
            cout << "Parse Error: " << key << " field does not exist" << endl;
            return "";
        }

        return (*doc)[key].GetString();
    }

    int getIntFromDOM(Document* doc, string key) {
        if (!doc->HasMember(key)) {
            cout << "Parse Error:" << key << " field does not exist" << endl;
            return -1;
        }

        return (*doc)[key].GetInt();
    }

    void OnConnection(const evpp::TCPConnPtr& conn) {
        LOG_INFO << conn->AddrToString() << " is " << (conn->IsConnected() ? "UP" : "DOWN");

        std::lock_guard<std::mutex> lock(mutex_);
        if (conn->IsConnected()) {
            connection_ = conn;

            //signin
            userid = "david";

            StringBuffer s;
            Writer<StringBuffer> writer(s);
            writer.StartObject();
            writer.Key("action");       writer.String("signin");
            writer.Key("userid");       writer.String(userid);
            writer.Key("password");       writer.String("david123");
            writer.EndObject();

            codec_.Send(connection_, s.GetString());
        }
        else {
            connection_.reset();
        }
    }

    void OnStringMessage(const evpp::TCPConnPtr&,
        const std::string& message) {
        Document document;
        document.Parse(message.c_str());

        cout << "<<<<" << message << endl;

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
            int success = getIntFromDOM(&document, "success");

            if (success == 1) {
                string token = getStrFromDOM(&document, "accessToken");

                this->accessToken = token;
                this->accessTime = chrono::system_clock::now();

                // Access the "licenses" array
                rapidjson::Value* jnode = &document["licenses"];
                const Value& licenses = *jnode;
                //for (const auto& license : licenses.GetArray()) {
                for (rapidjson::SizeType i = 0; i < licenses.Size(); ++i) {
                    ClientLicenseInfo new_license;
                    const Value& id = licenses[i]["id"];
                    const Value& type = licenses[i]["type"];
                    const Value& productName = licenses[i]["productName"];
                    const Value& licenseAccessToken = licenses[i]["accessToken"];

                    new_license.id = id.GetInt();
                    new_license.type = type.GetInt();
                    new_license.productName = productName.GetString();
                    new_license.accessToken = licenseAccessToken.GetString();
                    new_license.createdTime = chrono::system_clock::now();
                    licensesInfo.push_back(new_license);
                }
            }
            else if (success == 0) {
                string msg = getStrFromDOM(&document, "msg");
                cout << action << " : response => " << msg << endl;
            }
            else {
                cout << "Parse Error: success field does not exist" << endl;
            }
        }

        fprintf(stdout, "<<< %s\n", message.c_str());
        fflush(stdout);
    }

    evpp::TCPClient client_;
    LengthHeaderCodec codec_;
    std::mutex mutex_;
    evpp::TCPConnPtr connection_;

    string userid;
    string accessToken;
    vector<ClientLicenseInfo> licensesInfo;
    chrono::system_clock::time_point accessTime;

};

int main(int argc, char* argv[]) {
    /*
    // 1. Parse a JSON string into DOM.
    const char* json = "{\"project\":\"rapidjson\",\"stars\":10}";
    Document d;
    d.Parse(json);

    // 2. Modify it by DOM.
    Value& s = d["stars"];
    s.SetInt(s.GetInt() + 1);

    // 3. Stringify the DOM
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);

    // Output {"project":"rapidjson","stars":11}
    std::cout << buffer.GetString() << std::endl;
    */

#if 0
    StringBuffer s;
    Writer<StringBuffer> writer(s);
    writer.StartObject();
    writer.Key("action");       writer.String("signin");
    writer.Key("userid");       writer.String("david");
    writer.Key("password");       writer.String("david123");
    writer.EndObject();
    cout << s.GetString() << endl;

    Document document;  // Default template parameter uses UTF8 and MemoryPoolAllocator.
    document.Parse(s.GetString());
    //document.Parse(json);

    if (!document.IsObject()) {
        cout << "Parse Error" << endl;
        return 0;
    }
    if (document.HasMember("action"))
        cout << document["action"].GetString() << endl;

    return 0;
#endif

    evpp::EventLoopThread loop;
    loop.Start(true);
    std::string host = "127.0.0.1";
    std::string port = "9099";

    ChatClient client(loop.loop(), host + ":" + port);
    client.Connect();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            client.Disconnect();
            break;
        }
        client.Write(line);
    }
    loop.Stop(true);
    return 0;
}

