/*
  BAKUP.h -esp32 library that used portion of Internal Flash to backup
  frames(Data) in files.

  This lib used SPIFFS as file System, A portion of memory is used as Backup
  Storage.with Specific Numbers of files, The storage is used as circular
  queue of files.

  Dev: Infiplus Team
  Jan 2021
  */

#ifndef BAKUP_H
#define BAKUP_H

#define MAXFILES 50
#define FS_NOT_MOUNTED (0)
#define FILE_OPEN_FAILED (-1)
#define FILE_WRITE_SUCCESSFUL (1)

#include "FileSystem.h"
#include "SPIFFS.h"

// Structure to hold parsed backup entry data
typedef struct __attribute__((packed))
{
  char time[10];        // Time string e.g., "10:02"
  char pName[20];       // Pond name
  float doValue;        // DO in mg/L
  float tempValue;      // Temperature
  int fileIndex;        // Which BAK file this came from
} BackupEntry_t;

class CBackupStorage
{
private:
  /* data */
  int m_irPos;
  int m_iwPos;

public:
  CBackupStorage(/* args */);
  ~CBackupStorage();
  int InitilizeBS(FILESYSTEM *fileSystem);
  int writeInBS(FILESYSTEM *fileSystem, const char *frame);
  int readFromBS(FILESYSTEM *fileSystem, char *data);
  int moveToNextFile(FILESYSTEM *fileSystem);
  bool available(void);
  int clearAllFiles(FILESYSTEM *fileSystem);
  int countStoredFiles(FILESYSTEM *fileSystem);
  int clearNonBackupFiles(FILESYSTEM *fileSystem);
  int loadAllBackupEntries(FILESYSTEM *fileSystem, BackupEntry_t *entries, int maxEntries);
};

#endif