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
CPondConfig::CPondConfig(FILESYSTEM *fs)
{
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

        /*Length of config array is total Number of ponds*/
        m_u8TotalNoOfPonds = length;
        /* Parse the data in the file system */
        for (uint8_t i = 0; i < length; i++)
        {
          char data[150];
          bool isPondBoundariesDataAvailable = AVAILABLE;
          strncpy(data, slots[i], sizeof(data));
          debugPrintln(data);

          long locationVersion;
          char pondName[50];
          char pondId[50];
          int salinity;
          char locationId[50];
          int activeStatus = 0;

          sscanf(data, "%ld|%[^|]|%[^|]|%d|%[^|]|%d", &locationVersion, pondName, pondId, &salinity, locationId, &activeStatus);

          /* Check filesystem for pond boundary file */
          char pondFileName[100];
          snprintf(pondFileName, sizeof(pondFileName), "/%s.txt", pondName);

          int fileSize = _fileSystem->getFileSize(pondFileName);
          if (fileSize > 0)
          {
            /* File exists, read and check version */
            char *pondData = new char[fileSize + 1];
            int ret = _fileSystem->readFile(pondFileName, pondData);
            if (ret > 0)
            {
              DynamicJsonDocument pondDoc(fileSize * 2);
              DeserializationError err = deserializeJson(pondDoc, pondData);
              delete[] pondData;

              if (err.code() == DeserializationError::Ok)
              {
                if (pondDoc.containsKey("locationVersion"))
                {
                  long storedVersion = pondDoc["locationVersion"];
                  if (storedVersion != locationVersion)
                  {
                    debugPrintf("Version mismatch for pond %s: stored=%ld, new=%ld\n", pondName, storedVersion, locationVersion);
                    isPondBoundariesDataAvailable = NOT_AVAILABLE;
                  }
                }
                else
                {
                  debugPrintf("No version field in pond file %s\n", pondName);
                  isPondBoundariesDataAvailable = NOT_AVAILABLE;
                }
              }
              else
              {
                debugPrintf("Failed to parse JSON for pond %s\n", pondName);
                isPondBoundariesDataAvailable = NOT_AVAILABLE;
              }
            }
            else
            {
              delete[] pondData;
              debugPrintf("Failed to read pond file for %s\n", pondName);
              isPondBoundariesDataAvailable = NOT_AVAILABLE;
            }
          }
          else
          {
            /* File doesn't exist */
            debugPrintf("Pond file not found for %s\n", pondName);
            isPondBoundariesDataAvailable = NOT_AVAILABLE;
          }
          if (isPondBoundariesDataAvailable == NOT_AVAILABLE)
          {
            Serial.println("Pond Boundaries not available so setting the pond colors to blue");
            m_bGetPondBoundaries = true;
            updatedPondIds[pondId] = pondName;
            /*Initially when the pond config is loaded from file keep all the ponds color in blue representing
            the boundaries are not available for those ponds and later when boundaries are loaded show their state whether active or harvested*/
            m_pondStatusMap[pondName] = {!isPondBoundariesDataAvailable, activeStatus, POND_BOUNDARIES_NOT_AVAILABLE};
            /*Save only teh single pond status*/
            saveSinglePondStatusToFile(pondName);
          }
          else
          {
            Serial.printf("Pond boundaries available , active Status: %d \n", activeStatus);
            m_pondStatusMap[pondName] = {!isPondBoundariesDataAvailable, activeStatus, activeStatus};
          }
          // Update the PondSettingList array
          m_oPondList[i].m_iLocationVersion = locationVersion;
          strncpy(m_oPondList[i].m_cPondname, pondName, sizeof(m_oPondList[i].m_cPondname));
          strncpy(m_oPondList[i].m_cPondId, pondId, sizeof(m_oPondList[i].m_cPondId));
          m_oPondList[i].m_iSalinity = salinity;
          strncpy(m_oPondList[i].m_cLocationID, locationId, sizeof(m_oPondList[i].m_cLocationID));
          m_oPondList[i].PondActiveStatus = activeStatus;
        }
        /*Update Pond Status from Stored values if no file found save the local data in file*/
        if (!loadPondStatusFromFile())
        {
          Serial.println(" Saving the status to the pond map file");
          savePondStatusToFile();
        }
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
/*************************************************
 * save pond status in file system
 *************************************************/
bool CPondConfig::savePondStatusToFile()
{
  DynamicJsonDocument doc(4096); // Adjust size for number of ponds

  for (const auto &pair : m_pondStatusMap)
  {
    JsonObject obj = doc.createNestedObject(pair.first.c_str());
    obj["pb"] = pair.second.isBoundariesAvailable; // pb = pond boundaries available
    obj["isAct"] = pair.second.isActive;
    obj["pStat"] = pair.second.PondDataStatus;
  }

  std::string jsonStr;
  serializeJson(doc, jsonStr);

  debugPrintf("  pond status map (%d entries) to file\n", m_pondStatusMap.size());
  return _fileSystem->writeFile(PONDS_STATUS_CONFIG, jsonStr.c_str());
}

bool CPondConfig::saveSinglePondStatusToFile(const std::string &pondName)
{
  int size = _fileSystem->getFileSize(PONDS_STATUS_CONFIG);
  if (size <= 0)
  {
    debugPrintln("PONDS_STATUS_CONFIG not found or empty");
    return false;
  }

  // Allocate buffer on heap for file content
  const size_t bufferSize = 2048; // Adjust based on expected file size
  char *buffer = new char[bufferSize];
  if (!buffer)
  {
    debugPrintf("Heap allocation failed for buffer!\n");
    return false;
  }
  memset(buffer, 0, bufferSize);

  // Read existing pond status file
  if (_fileSystem->readFile(PONDS_STATUS_CONFIG, buffer) <= 0)
  {
    debugPrintf("Pond status file not found. Creating a new one.\n");
    strcpy(buffer, "{}"); // Start fresh if file missing
  }

  // Convert to std::string for easier JSON handling
  std::string jsonContent(buffer);

  // Free the buffer now that we have std::string
  delete[] buffer;

  // Allocate JSON document on heap
  DynamicJsonDocument *doc = new DynamicJsonDocument(4096); // adjust if needed
  if (!doc)
  {
    debugPrintf("Heap allocation failed for JSON document!\n");
    return false;
  }

  // Parse existing JSON
  DeserializationError error = deserializeJson(*doc, jsonContent.c_str());
  if (error)
  {
    debugPrintf("Failed to parse existing pond status JSON: %s\n", error.c_str());
    delete doc;
    return false;
  }

  // Ensure the pond exists in memory map
  auto it = m_pondStatusMap.find(pondName);
  if (it == m_pondStatusMap.end())
  {
    debugPrintf("Pond '%s' not found in m_pondStatusMap.\n", pondName.c_str());
    delete doc;
    return false;
  }

  // Update or create JSON entry for this pond
  JsonObject obj = (*doc)[pondName.c_str()].to<JsonObject>();
  obj["pb"] = it->second.isBoundariesAvailable;
  obj["isAct"] = it->second.isActive;
  obj["pStat"] = it->second.PondDataStatus;

  // Serialize updated JSON back to string
  std::string newJson;
  serializeJson(*doc, newJson);

  // Free JSON document
  delete doc;

  // Write back to file
  bool result = _fileSystem->writeFile(PONDS_STATUS_CONFIG, newJson.c_str());
  debugPrintf("Saved single pond '%s' status to file: %s\n",
              pondName.c_str(), result ? "SUCCESS" : "FAILURE");

  return result;
}

/*************************************************
 * Load pond status from file system
 *************************************************/
bool CPondConfig::loadPondStatusFromFile()
{
  int size = _fileSystem->getFileSize(PONDS_STATUS_CONFIG);
  if (size <= 0)
  {
    debugPrintln("PONDS_STATUS_CONFIG not found or empty");
    return false;
  }

  char *buffer = new char[size + 1];
  if (!_fileSystem->readFile(PONDS_STATUS_CONFIG, buffer))
  {
    debugPrintln("Failed to read PONDS_STATUS_CONFIG");
    delete[] buffer;
    return false;
  }

  DynamicJsonDocument doc(size * 2);
  DeserializationError err = deserializeJson(doc, buffer);
  delete[] buffer;

  if (err)
  {
    debugPrintf("JSON deserialization failed: %s\n", err.c_str());
    return false;
  }
  /*update pond st*/
  for (JsonPair kv : doc.as<JsonObject>())
  {
    const char *pondName = kv.key().c_str();
    JsonObject obj = kv.value();
    m_pondStatusMap[pondName].isActive = obj["isAct"];
    m_pondStatusMap[pondName].PondDataStatus = obj["pStat"];
  }

  debugPrintf("Loaded %d pond statuses from file\n", m_pondStatusMap.size());
  return true;
}

void CPondConfig::resetAllPondDataStatus()
{
  debugPrintln("Resetting all PondDataStatus values to 1 if they are active");

  for (auto &pair : m_pondStatusMap)
  {
    if (pair.second.isActive)
    {
      pair.second.PondDataStatus = 1;
    }
  }
  savePondStatusToFile();
}