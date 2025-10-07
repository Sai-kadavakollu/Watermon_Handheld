#ifndef CPondConfig_H
#define CPondConfig_H

#include <stdint.h>
#include <ArduinoJson.h>
#include <FILESYSTEM.h>
#include <map>
#include <string>
#include <iostream>
#include <set>     // Include this header for using set
#include <cstring> // Include for C-style string functions like strcpy and strncpy
#include <cstdio>
#include <vector>

#define TOTAL_PONDS 60
#define FILENAME_IDSCONFIG "/idsConfig.txt"
#define PONDS_STATUS_CONFIG "/PondsActiveFile.txt"

/*POND_MAP_FRAME_STORED_STATUS*/
#define PONDMAP_VALUE_NOT_ACTIVE 0
#define PONDMAP_VALUE_YET_TO_BE_TAKEN 1
#define PONDMAP_VALUE_FRAME_STORED_TO_BACKUP 2
#define PONDMAP_VALUE_FRAME_SENT_SUCESSFULLY 3
#define PONDMAP_VALUE_TAKEN_BUT_ERROR 4
#define POND_BOUNDARIES_NOT_AVAILABLE 5

#define AVAILABLE 0
#define NOT_AVAILABLE 1

using namespace std;

struct PondInfo
{
    bool isBoundariesAvailable;
    int isActive;
    int PondDataStatus;
    bool operator==(const PondInfo &other) const
    {
        return PondDataStatus == other.PondDataStatus; // Add other field comparisons if needed
    }
};

class CPond
{
private:
public:
    char m_cPondId[50];
    char m_cPondname[10];
    int m_iLocationVersion;
    int m_iSalinity;
    char m_cLocationID[50];
    uint8_t PondActiveStatus;
    uint8_t PondValueStatus;
};

class CPondConfig
{
private:
    FILESYSTEM *_fileSystem;

public:
    /* Construct */
    CPondConfig(FILESYSTEM *fs); // Constructor
    /* Destruct */
    ~CPondConfig();
    char m_u64currentPondId[50];
    char m_cLocationId[50];
    char m_cCurrentPondname[10];
    int m_iCurrentSalinity;
    int64_t m_i64ConfigIdsVersion;
    uint8_t m_u8TotalNoOfPonds;

    // std::map<std::string, int> m_locationVersions;

    std::map<std::string, std::string> updatedPondIds;
    /*Map to store the pondActive status and capture frame status related to that pond, P1:{1,3}*/
    std::map<std::string, PondInfo> m_pondStatusMap;

    char m_cTenantId[20];
    bool m_bGetPondBoundaries = false;
    int m_iOffset;
    CPond m_oPondList[TOTAL_PONDS];
    double m_dPondSettingVer;
    int loadPondConfig();
    bool savePondStatusToFile();
    bool saveSinglePondStatusToFile(const std::string &pondName);
    bool loadPondStatusFromFile();
    void resetAllPondDataStatus();
};

#endif