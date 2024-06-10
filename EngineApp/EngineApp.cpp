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

        // initialize
        licensesInfo.id = 1;
        licensesInfo.type = 1;
        licensesInfo.productName = "Product 1";
        licensesInfo.accessToken = "b0487e1c25f36953c2572415124a25638514c3a82677ee5da7cc43f4188e0ef6";        
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
            StringBuffer s;
            Writer<StringBuffer> writer(s);
            writer.StartObject();
            writer.Key("action");       writer.String("connectLicense");
            writer.Key("id");       writer.Int(licensesInfo.id);
            writer.Key("accessToken");       writer.String(licensesInfo.accessToken);
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
        if (action == "connectLicense") {
            int success = getIntFromDOM(&document, "success");

            if (success == 1) {
                string token = getStrFromDOM(&document, "accessToken");
                licensesInfo.accessToken = token;
                licensesInfo.accessTime = chrono::system_clock::now();

                // Access the "licenses" array
                cout << action << " : response => OK "<< endl;
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

    ClientLicenseInfo licensesInfo;
};

int main(int argc, char* argv[]) {
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

