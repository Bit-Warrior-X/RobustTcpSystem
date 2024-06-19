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

class ChatEngine {

private:
    evpp::TCPClient client_;
    LengthHeaderCodec codec_;
    std::mutex mutex_;
    evpp::TCPConnPtr connection_;

    string      userid;
    int         licenseid;
    std::string accessToken;

public:
    ChatEngine(evpp::EventLoop* loop, const std::string& serverAddr)
        : client_(loop, serverAddr, "ChatEngine"),
        codec_(std::bind(&ChatEngine::OnStringMessage, this, std::placeholders::_1, std::placeholders::_2)) {
        client_.SetConnectionCallback(
            std::bind(&ChatEngine::OnConnection, this, std::placeholders::_1));
        client_.SetMessageCallback(
            std::bind(&LengthHeaderCodec::OnMessage, &codec_, std::placeholders::_1, std::placeholders::_2));
    }

    void Connect(string user, int id, std::string token) {
        userid = user;
        licenseid = id;
        accessToken = token;

        if (connection_) {
            codec_.Send(connection_, GenerateConnectionRequestMessage());
        } else 
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

    string GenerateConnectionRequestMessage() {
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        writer.StartObject();
        writer.Key("action");       writer.String("connectLicense");
        writer.Key("userid");       writer.String(userid);
        writer.Key("licenseid");    writer.Int(licenseid);
        writer.Key("accessToken");  writer.String(accessToken);
        writer.EndObject();
        return s.GetString();
    }

    void OnConnection(const evpp::TCPConnPtr& conn) {
        LOG_INFO << conn->AddrToString() << " is " << (conn->IsConnected() ? "UP" : "DOWN");

        std::lock_guard<std::mutex> lock(mutex_);
        if (conn->IsConnected()) {
            connection_ = conn;
            codec_.Send(connection_, GenerateConnectionRequestMessage());
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
};

int main(int argc, char* argv[]) {

    std::string hostaddr; // 127.0.0.1:9099
    std::string userid;
    int licenseid;
    std::string token;
    
    if (argc != 5) {
        cout << "Invaild argument count" << endl;
        return 0;
    }

    evpp::EventLoopThread loop;
    loop.Start(true);

    hostaddr  = argv[1];
    userid    = argv[2];
    licenseid = atoi(argv[3]);
    token     = argv[4];

    ChatEngine client(loop.loop(), hostaddr);
    
    client.Connect(userid, licenseid, token);

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

