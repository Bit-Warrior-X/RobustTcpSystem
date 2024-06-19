#ifndef UTILS_H
#define UTILS_H
#include <iostream>
#include <string>
#include <mutex>

using namespace std;

#define RC_OK             1
#define RC_FAILED         0

#define ACTION_SIGIN      "sign"

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

enum {
    RESPONSE_SERVER_ERR,
    RESPONSE_RETURN_ERR,
    RESPONSE_RETURN_SUC
};
#endif

#pragma once
