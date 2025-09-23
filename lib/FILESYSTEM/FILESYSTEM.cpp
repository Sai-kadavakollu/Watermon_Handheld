/* 
  FileSystem.cpp - esp32 library that used portion of Internal Flash to backup
  frames(Data) in files.

  This lib used SPIFFS as file System, A portion of memory is used as Backup
  Storage.with Specific Numbers of files, The storage is used as circular 
  queue of files.

  Dev: Infiplus Team 
  Jan 2021  
  */

#include "FILESYSTEM.h"
#include "SPIFFS.h"

/**
 * constructor
 */
FILESYSTEM::FILESYSTEM()
{
    fsMounted = false;
}

/**
 * destructor
 */
FILESYSTEM::~FILESYSTEM()
{
    fsMounted = false;
}

/**
 * Write data to file,If file not preset Create and Write 
 * @param path Path of file
 * @param data String to be written
 * @return FILE/FS ERRORS
 */
int FILESYSTEM::writeFile(const char *path, const char *message)
{
    if (fsMounted)
    {
        File file = SPIFFS.open(path, FILE_WRITE);
        if (!file)
        {
            return FILE_OPEN_FAILED;
        }
        if (file.print(message))
        {
            return FILE_WRITE_SUCCESSFUL;
        }
        else
        {
            return FILE_WRITE_FAILED;
        }
    }
    return FS_NOT_MOUNTED;
}

/**
 * Read data from file
 * @param path Path of file
 * @param data Pointer to buff 
 * @return File Error , No of Bytes read
 */
int FILESYSTEM::readFile(const char *path, char *data)
{
    if (fsMounted)
    {
        File file = SPIFFS.open(path);
        int bRead = 0;
        if (!file || file.isDirectory())
        {
            return FILE_OPEN_FAILED;
        }

        size_t fileSize = file.size();

        while (file.available())
        {
            (*data) = file.read();
            data++;
            bRead++;

            if (bRead > fileSize + 1)
            {
                return FILE_READ_OVERFLOW;
            }
        }
        (*data) = '\0';
        return bRead;
    }
    return FS_NOT_MOUNTED;
}

/**
 * Mount SPIFFS
 * @param void
 * @return success/ fail bool
 */
bool FILESYSTEM::begin()
{
    if (SPIFFS.begin(true))
    {
        fsMounted = true;
        return true;
    }
    else
    {
        /*SPIFFS Mount Failed*/;
        fsMounted = false;
        return false;
    }
}

/**
 * check if mounted and read the file size
 * @param fname file name
 * @return file Size
 */
int FILESYSTEM::getFileSize(const char *fname)
{
    if (fsMounted)
    {
        File file = SPIFFS.open(fname);
        if (!file || file.isDirectory())
        {
            /*File Not Found*/
            return 0;
        }
        return file.size();
    }
    return 0;
}

bool FILESYSTEM::isMounted(void)
{
    return fsMounted;
}

// File FILESYSTEM::openFile(const char *fname) 
// {
//     return (SPIFFS.open(fname));
// }
