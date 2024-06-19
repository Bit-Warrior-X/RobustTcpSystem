#pragma once

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

#include "Utils.h"

using namespace std;
using namespace rapidjson;

class ChatClient
{
private:
    evpp::TCPClient client_;
    LengthHeaderCodec codec_;
    std::mutex mutex_;
    evpp::TCPConnPtr connection_;
    evpp::EventLoopThread gLoop;

    string hostAddress;
    string userid;
    string userpass;
    string accessToken;
    vector<LicenseInstance> licensesInfo;
    chrono::system_clock::time_point accessTime;

    std::function<void(const std::string&, int status)> login_message_callback_;
    std::function<void(int licenseid, int type, string& msg)> main_message_callback_;
    
public:
    ChatClient(evpp::EventLoop* loop, const std::string& serverAddr)
        : client_(loop, serverAddr, "ChatClient"),
        codec_(std::bind(&ChatClient::OnStringMessage, this, std::placeholders::_1, std::placeholders::_2)) 
    {
        hostAddress = serverAddr;
        client_.SetConnectionCallback(
            std::bind(&ChatClient::OnConnection, this, std::placeholders::_1));
        client_.SetMessageCallback(
            std::bind(&LengthHeaderCodec::OnMessage, &codec_, std::placeholders::_1, std::placeholders::_2));
    }
    ~ChatClient();

    void Connect(std::string id, std::string password);
    void Disconnect();
    void Write(const evpp::Slice& message);

    void SetLoginMessageCallback(const std::function<void(const std::string&, int status)>& callback);
    void SetMainMessageCallback(const std::function<void(int licenseid, int type, string& msg)>& callback);

    string GetHostAddress();
    string GetUserID();
    vector <LicenseInstance> GetLicenseList();
    LicenseInstance& GetLicenseInstance(int licenseID);
    int GetNetworkStatus();

private:
    string GetStrFromDOM(Document* doc, string key);
    int GetIntFromDOM(Document* doc, string key);

    void OnConnection(const evpp::TCPConnPtr& conn);
    void OnStringMessage(const evpp::TCPConnPtr&, const std::string& message);

    string GenerateConnectionRequestMessage();
    
    void ParseSignInMessage(Document *document);
    void ParseCommandMessage(Document *document);
};

