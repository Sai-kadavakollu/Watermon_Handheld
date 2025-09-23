/*
  PM_APP.cpp - Application file is used to read the registor values of energy meter ic , calculate the suggested/ required capbank value and also used for detection of
               power source

  Dev: Infiplus Team
  May 2021
*/

#include "CPondConfig.h"
#include "stdio.h"
#include <ArduinoJson.h>

// #define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#define debugPrintlnf(...) Serial.println(F(__VA_ARGS__))
#else
#define debugPrint(...)    // blank line
#define debugPrintln(...)  // blank line
#define debugPrintf(...)   // blank line
#define debugPrintlnf(...) // blank line
#endif


/* Construct */
CPondConfig::CPondConfig(FILESYSTEM *fs) {
  _fileSystem = fs;
}
/* Destruct */
CPondConfig::~CPondConfig() {}
/************************************************************
 * Load Pond Setting From File in local Veriables
 *************************************************************/
int CPondConfig::loadPondConfig()
{
  // Serial.println(" In the loading Pondconfiguration");
  int ret = 0;
  int FileSize = _fileSystem->getFileSize(FILENAME_IDSCONFIG);

  if (FileSize == 0)
  {
    debugPrintln("Wrong Setting file size...");
    return 0;
  }
  char rdata[FileSize];
  /* Read File into data stream */
  ret = _fileSystem->readFile(FILENAME_IDSCONFIG, rdata);
  // Serial.print("############################################### file details in load pond config ###############################################");
  // Serial.println(rdata);
  if (ret > 0)
  {
    DynamicJsonDocument configInfo(FileSize * 2);
    /* Deserialize the file JSON */
    DeserializationError err = deserializeJson(configInfo, rdata);
    if (err.code() == DeserializationError::Ok)
    {
      /* Read Pond Setting Version */
      if (configInfo.containsKey("version"))
      {
        m_i64ConfigIdsVersion = configInfo["version"];
        debugPrintln("@@ version found in file");
      }
      else
      {
        m_i64ConfigIdsVersion = 0;
        debugPrintln("@@ version not found in file");
      }

      if (configInfo.containsKey("tenantId"))
      {
        strcpy(m_cTenantId, configInfo["tenantId"]);
        debugPrintln("@@ tenantId found in file");
      }
      else
      {
        strcpy(m_cTenantId, "none");
        debugPrintln("@@ tenantId not found in file");
      }

      if (configInfo.containsKey("offset"))
      {
        m_iOffset = configInfo["offset"];
        debugPrintln("@@ offset found in file");
      }
      else
      {
        m_iOffset = 330;
        debugPrintln("@@ offset not found in file");
      }

      /* Read Pond-wise Pond data */
      if (configInfo.containsKey("config"))
      {
        bool updateFile = false;
        JsonArray slots = configInfo["config"];
        int length = slots.size();
        debugPrintf("length: %d\n", length);
        if (length == 1)
        {
          char data[150];
          strncpy(data, slots[0], sizeof(data));
          debugPrintln(data);

          long locationVersion;
          char pondName[50];
          char pondId[50];
          int salinity;
          char locationId[50];

          sscanf(data, "%ld|%[^|]|%[^|]|%d|%[^|]", &locationVersion, pondName, pondId, &salinity, locationId);
          strncpy(m_cCurrentPondname, pondName, sizeof(m_cCurrentPondname));
          strncpy(m_u64currentPondId, pondId, sizeof(m_u64currentPondId));
          strncpy(m_cLocationId, locationId, sizeof(m_cLocationId));
          m_iCurrentSalinity = salinity;
          return 1;
        }

        /* Read location versions once */
        readLocationVersions();

        /* Set to track processed pondIds */
        set<string> processedPondIds;

        /* Parse the data in the file system */
        for (uint8_t i = 0; i < length; i++)
        {
          char data[150];
          strncpy(data, slots[i], sizeof(data));
          debugPrintln(data);

          long locationVersion;
          char pondName[50];
          char pondId[50];
          int salinity;
          char locationId[50];
  
          sscanf(data, "%ld|%[^|]|%[^|]|%d|%[^|]", &locationVersion, pondName, pondId, &salinity, locationId);
           
          loadPondStatusFromFileOrConfig(slots);
          /* Track the processed pondId */
          processedPondIds.insert(pondId);

          /* Check whether the pondId is present in the map, if not add it */
          if (m_locationVersions.find(pondId) != m_locationVersions.end())
          {
            if (m_locationVersions[pondId] != locationVersion)
            {
              debugPrintf("Updating version for pondId: %s\n", pondId);
              m_locationVersions[pondId] = locationVersion;
              updatedPondIds[pondId] = pondName;
              m_bGetPondBoundaries = true;
              updateFile = true;
            }
          }
          else
          {
            debugPrintf("Adding new pondId: %s with version: %ld\n", pondId, locationVersion);
            m_locationVersions[pondId] = locationVersion;
            updatedPondIds[pondId] = pondName;
            m_bGetPondBoundaries = true;
            updateFile = true;
          }

          // Update the PondSettingList array
          m_oPondSettingList[i].m_iLocationVersion = locationVersion;
          strncpy(m_oPondSettingList[i].m_cPondname, pondName, sizeof(m_oPondSettingList[i].m_cPondname));
          strncpy(m_oPondSettingList[i].m_cPondId, pondId, sizeof(m_oPondSettingList[i].m_cPondId));
          m_oPondSettingList[i].m_iSalinity = salinity;
          strncpy(m_oPondSettingList[i].m_cLocationID, locationId, sizeof(m_oPondSettingList[i].m_cLocationID));
        }
        /*BEGIN*/
        /*Below Lines of code belongs to count total No of Active Ponds and Total no of ponds available in the config*/
          m_u8TotalNoOfPonds = m_pondStatusMap.size();
          int activeCount = 0;
          for (const auto& pair : m_pondStatusMap) {
              if (pair.second.ActiveStatus) activeCount++;
          }
          m_u8TotalNoOfActivePonds = activeCount;
          debugPrintf("\n Total Ponds Mapped: %d\n", m_u8TotalNoOfPonds);
          debugPrintf("Active Ponds: %d\n", m_u8TotalNoOfActivePonds);
        /*END*/

        /* Remove entries from the map that are not in the processedPondIds set */
        for (auto item = m_locationVersions.begin(); item != m_locationVersions.end();)
        {
          if (processedPondIds.find(item->first) == processedPondIds.end())
          {
            debugPrintf("Removing unused pondId: %s\n", item->first.c_str());
            item = m_locationVersions.erase(item);
          }
          else
          {
            ++item;
          }
        }

        if (updateFile)
        {
          /*add the data into the file system*/
          DynamicJsonDocument doc(2500);
          JsonObject config = doc.createNestedObject("config");
          for (const auto &entry : m_locationVersions)
          {
            config[entry.first] = entry.second;
          }
          string jsonStr;
          serializeJson(doc, jsonStr);
          _fileSystem->writeFile(LOCATION_VERSIONS, jsonStr.c_str());
        }

        /* Output the updated pondIds */
        // debugPrintln("\nUpdated PondIds and PondNames:");
        // for (const auto &pair : updatedPondIds)
        // {
        //   debugPrintf("PondId: %s, PondName: %s\n", pair.first.c_str(), pair.second.c_str());
        // }
      }
      else
      {
        debugPrintln("No config found in the file...");
        return 0;
      }
    }
  }
  else
  {
    debugPrintln(" The file is empty ");
  }
  return 1;
}

/************************************************************
 * Read Pond Setting From File
 *************************************************************/
int CPondConfig::readLocationVersions()
{
  char rdata[1024];
  int ret = 0;
  int Size = _fileSystem->getFileSize(LOCATION_VERSIONS);
  debugPrintf("Size of the file is %d\n", Size);

  if (Size >= sizeof(rdata)) // Ensure file size is within the buffer's capacity
  {
    debugPrintln("Wrong Setting file size...");
    return 0;
  }

  ret = _fileSystem->readFile(LOCATION_VERSIONS, rdata);
  if (ret > 0)
  {
    DynamicJsonDocument configInfo(sizeof(rdata)); // Allocate a JSON document based on buffer size

    /* Deserialize the JSON content */
    DeserializationError err = deserializeJson(configInfo, rdata);
    if (err.code() == DeserializationError::Ok)
    {
      if (configInfo.containsKey("config"))
      {
        JsonObject configArray = configInfo["config"];

        for (JsonPair obj : configArray)
        {
          string key = obj.key().c_str();
          int value = obj.value().as<int>();

          // Store the key-value pair in the map
          m_locationVersions[key] = value;
        }
      }
      else
      {
        debugPrintln("No config in JSON");
      }
    }
    else
    {
      debugPrintln("Failed to parse JSON");
    }
  }
  else
  {
    debugPrintf("Failed to read file, ret = %d\n", ret);
  }
  return 0;
}


bool CPondConfig::savePondStatusToFile() {
    DynamicJsonDocument doc(2048);  // Adjust size for number of ponds

    for (const auto& pair : m_pondStatusMap) {
        JsonObject obj = doc.createNestedObject(pair.first.c_str());
        obj["active"] = pair.second.ActiveStatus;
        obj["backup"] = pair.second.Backup;
    }

    std::string jsonStr;
    serializeJson(doc, jsonStr);

    debugPrintf("Saving pond status map (%d entries) to file\n", m_pondStatusMap.size());
    return _fileSystem->writeFile(PONDS_STATUS_CONFIG, jsonStr.c_str());
}


bool CPondConfig::loadPondStatusFromFile() {
    int size = _fileSystem->getFileSize(PONDS_STATUS_CONFIG);
    if (size <= 0) {
        debugPrintln("PONDS_STATUS_CONFIG not found or empty");
        return false;
    }

    char *buffer = new char[size + 1];
    if (!_fileSystem->readFile(PONDS_STATUS_CONFIG, buffer)) {
        debugPrintln("Failed to read PONDS_STATUS_CONFIG");
        delete[] buffer;
        return false;
    }

    DynamicJsonDocument doc(size * 2);
    DeserializationError err = deserializeJson(doc, buffer);
    delete[] buffer;

    if (err) {
        debugPrintf("JSON deserialization failed: %s\n", err.c_str());
        return false;
    }

    m_pondStatusMap.clear();

    for (JsonPair kv : doc.as<JsonObject>()) {
        const char* pondName = kv.key().c_str();
        JsonObject obj = kv.value();
        bool active = obj["active"];
        uint8_t backup = obj["backup"];
        m_pondStatusMap[pondName] = { active, backup };
    }

    debugPrintf("Loaded %d pond statuses from file\n", m_pondStatusMap.size());
    return true;
}


bool CPondConfig::loadPondStatusFromFileOrConfig(JsonArray configArray) {
    if (loadPondStatusFromFile() && !m_bMapNeedToBeUpdated) {
        debugPrintln("Loaded pond status from saved file");
        return true;
    }

    debugPrintln("Building pond map from config array...");

    for (JsonVariant v : configArray) {
        const char* entry = v.as<const char*>();
        char pondName[50];
        int activeStatus = 0;

        // Parse pondName and activeStatus
        sscanf(entry, "%*[^|]|%[^|]|%*[^|]|%*d|%*[^|]|%d", pondName, &activeStatus);
        //if config need to be updated, only update the active status not the backup one
        bool isActive = (activeStatus == 1);
        m_pondStatusMap[pondName] = { isActive, static_cast<uint8_t>(isActive) };
    }
    
    // Save initial state for persistence
    savePondStatusToFile();
    m_bMapNeedToBeUpdated = false;
    return true;
}

void CPondConfig::resetAllBackupToOne() {
    debugPrintln("Resetting all backup values to 1");

    for (auto& pair : m_pondStatusMap) {
        if (pair.second.ActiveStatus) {
            pair.second.Backup = 1;
        }
    }
    savePondStatusToFile();
}