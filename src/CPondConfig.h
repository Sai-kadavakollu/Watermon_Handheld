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
#define LOCATION_VERSIONS "/locationVersions.txt"
#define FILENAME_IDSCONFIG "/idsConfig.txt"
#define PONDS_STATUS_CONFIG "/PondsActiveFile.txt"

using namespace std;

struct PondInfo {
    bool ActiveStatus;
    uint8_t Backup;
    bool operator==(const PondInfo& other) const {
        return Backup == other.Backup;  // Add other field comparisons if needed
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
};

class CPondConfig
{
private:
    FILESYSTEM *_fileSystem;
public:
    /* Construct */
    CPondConfig(FILESYSTEM *fs);  // Constructor
    /* Destruct */
    ~CPondConfig();
    char m_u64currentPondId[50];
    char m_cLocationId[50];
    char m_cCurrentPondname[10];
    int m_iCurrentSalinity;
    int64_t m_i64ConfigIdsVersion;
    uint8_t m_u8TotalNoOfActivePonds;
    uint8_t m_u8TotalNoOfPonds;
    bool m_bMapNeedToBeUpdated = false;

    std::map<std::string, int> m_locationVersions;

    std::map<std::string, std::string> updatedPondIds;
    /*Map to store the pondActive status and capture frame status related to that pond, P1:{1,3}*/
    std::map<std::string, PondInfo> m_pondStatusMap;    

    char m_cTenantId[20];
    bool m_bGetPondBoundaries = false;
    int m_iOffset;
    CPond m_oPondSettingList[TOTAL_PONDS];
    double m_dPondSettingVer;
    int loadPondConfig();
    int readLocationVersions();
    bool savePondStatusToFile();
    bool loadPondStatusFromFile();
    bool loadPondStatusFromFileOrConfig(ArduinoJson::JsonArray  configArray);
    void resetAllBackupToOne();
};


#endif