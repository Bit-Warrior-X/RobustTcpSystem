// ServerApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "ChatServer.h"

map<string, UserInfo> gUserMap;
map<int, LicenseInstance> gLicenseMap;
vector <ConnectionInfo> gConnectionList;

void InitializeDatabase()
{
    GameInfo gameInfo0 = { 0, "GameA" };
    GameInfo gameInfo1 = { 1, "GameB" };
    GameInfo gameInfo2 = { 2, "GameC" };

    LicenseDefinition licenseDefinition0 = { 0, gameInfo0, ELicenseVariationType_Lite };
    LicenseDefinition licenseDefinition1 = { 1, gameInfo0, ELicenseVariationType_Regular };
    LicenseDefinition licenseDefinition2 = { 2, gameInfo1, ELicenseVariationType_Lite };
    LicenseDefinition licenseDefinition3 = { 3, gameInfo1, ELicenseVariationType_Regular };
    LicenseDefinition licenseDefinition4 = { 4, gameInfo2, ELicenseVariationType_Lite };
    LicenseDefinition licenseDefinition5 = { 5, gameInfo2, ELicenseVariationType_Regular };


    LicenseInstance license0 = { 0, licenseDefinition0, "moto", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0};
    LicenseInstance license1 = { 1, licenseDefinition0, "moto", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license2 = { 2, licenseDefinition3, "moto", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license3 = { 3, licenseDefinition4, "moto", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license4 = { 4, licenseDefinition4, "moto", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license5 = { 5, licenseDefinition1, "victor", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license6 = { 6, licenseDefinition1, "victor", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license7 = { 7, licenseDefinition1, "victor", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license8 = { 8, licenseDefinition4, "victor", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };
    LicenseInstance license9 = { 9, licenseDefinition4, "victor", "", chrono::system_clock::time_point(), chrono::system_clock::time_point() , 0 };


    gLicenseMap.insert(std::make_pair(license0.id, license0));
    gLicenseMap.insert(std::make_pair(license1.id, license1));
    gLicenseMap.insert(std::make_pair(license2.id, license2));
    gLicenseMap.insert(std::make_pair(license3.id, license3));
    gLicenseMap.insert(std::make_pair(license4.id, license4));
    gLicenseMap.insert(std::make_pair(license5.id, license5));
    gLicenseMap.insert(std::make_pair(license6.id, license6));
    gLicenseMap.insert(std::make_pair(license7.id, license7));
    gLicenseMap.insert(std::make_pair(license8.id, license8));
    gLicenseMap.insert(std::make_pair(license9.id, license9));

    UserInfo user1 = { 1, "moto",   "Moto Zocker",     "abc123", {0,1,2,3,4}, "", chrono::system_clock::time_point(), nullptr };
    UserInfo user2 = { 2, "victor", "Victor Kiselev",  "abc123", {5,6,7,8,9}, "", chrono::system_clock::time_point(), nullptr };

    gUserMap.insert(std::make_pair(user1.userid, user1));
    gUserMap.insert(std::make_pair(user2.userid, user2));
}

int main(int argc, char* argv[]) {
    evpp::EventLoop loop;
    std::string addr = std::string("0.0.0.0:9099");

    InitializeDatabase();

    ChatServer server(&loop, addr);
    server.Start();
    loop.Run();
    return 0;
}

