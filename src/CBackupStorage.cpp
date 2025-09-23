/* 
  BAKUP.cpp - esp32 library that used portion of Internal Flash to backup
  frames(Data) in files.

  This lib used SPIFFS as file System, A portion of memory is used as Backup
  Storage.with Specific Numbers of files, The storage is used as circular 
  queue of files.

  Dev: Infiplus Team 
  Jan 2021  
  */

 #include "CBackupStorage.h"

// #define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#define debugPrintlnf(...) Serial.println(F(__VA_ARGS__))
#else
#define debugPrint(...)    //blank line
#define debugPrintln(...)  //blank line
#define debugPrintf(...)   //blank line
#define debugPrintlnf(...) //blank line
#endif
 /**
 * constructor
 */
 CBackupStorage::CBackupStorage()
 { 
    m_irPos = 0;
    m_iwPos = 0;
 }

/**
 * destructor
 */
 CBackupStorage::~CBackupStorage()
 {
  }


/********************************************************************
 * Check for availability of required number of files in Storage,
 * if file not present,Create files 
 * @param[in] void
 * @return File Error/FS error
 ******************************************************************/
int CBackupStorage::InitilizeBS(FILESYSTEM *fileSystem)
{
    int pVal = 0;
    int cVal = 0;
    long Wtime[MAXFILES] = {0};
    long maxT,minT;

    if(fileSystem->isMounted())
    { 
        for(int i = 0; i < MAXFILES; i++)
        {
            char fname[20]={0};    
            sprintf(fname,"/BAK_%d.txt",i);
            File file = SPIFFS.open(fname);//fileSystem->openFile(fname);
            if(!file || file.isDirectory())
            {
                /* if file not Found Creating New file */
                fileSystem->writeFile(fname,"No Data");
            }
            else
            {
                debugPrint("Size of File: ");
                debugPrint(file.name());
                debugPrint("\tSIZE: ");
                debugPrintln(file.size());
            }
        }

        /* Get read and Write position from the storage*/
        m_irPos = 0;
        m_iwPos = 0;
        for(int i = 0; i< MAXFILES; i++)
        {
            char fname[20]={0};
            sprintf(fname,"/BAK_%d.txt",i);
            File file = SPIFFS.open(fname);//fileSystem->openFile(fname);
            if(!file || file.isDirectory())
            {
                debugPrintln("-file not Found");
            }
            else
            {
                Wtime[i] = file.getLastWrite();
                cVal = ((file.size() > 10)? 1 : 0);
                if ((pVal == 0) && (cVal == 1)) { m_irPos = i;}
                else if ((pVal == 1) && (cVal == 0)) { m_iwPos = i;}
                pVal = cVal;
            }          
        }
         /*There is data in all the files, Memory full*/
        if(m_irPos == 0 && m_iwPos == 0 && cVal == 1)
        {
            debugPrintln("Geting Pos With last Write Time");
            minT = maxT =  Wtime[0] ;
            for(int i = 1; i< MAXFILES; i++)
            {
                if(Wtime[i] < minT)
                {
                    minT = Wtime[i];
                    m_irPos = i;
                }
                else if(Wtime[i] >= maxT)
                {
                    maxT = Wtime[i];
                    m_iwPos = (i+1) % MAXFILES;
                }
            }

            if(m_iwPos == m_irPos)
            {
                m_irPos = (m_irPos+1)% MAXFILES;   
            }  
            debugPrint("Min : ");
            debugPrintln(minT);
            debugPrint("max : ");
            debugPrintln(maxT);
        } 
        debugPrint("rPos : ");
        debugPrintln(m_irPos);
        debugPrint("wPos : ");
        debugPrintln(m_iwPos);    
        return 1; 
    }
    debugPrintln("File System Not Mounted");
    return FS_NOT_MOUNTED;   
}

/********************************************************************
 * store data in available file position in Storage,
 * @param[in] frame Data to be saved in file
 * @return File Error/FS error
 *******************************************************************/
int CBackupStorage::writeInBS(FILESYSTEM *fileSystem, const char *frame)
{
    char fname[20]={0};
    sprintf(fname,"/BAK_%d.txt",m_iwPos);
    debugPrintln(fname);
    if(fileSystem->writeFile(fname,frame))
    {
        m_iwPos = (m_iwPos + 1)% MAXFILES;
        if(m_iwPos == m_irPos)
        {
            debugPrintln("Pushing rPos ahead of wPos");
            m_irPos = (m_iwPos+1)%MAXFILES;
            debugPrintln(m_iwPos);
            debugPrintln(m_irPos);
        }
        return 1;
    } 
    else
    {
        debugPrintln("Backup to storage failed");    
    } 
    return FS_NOT_MOUNTED; 
}

/*******************************************************************
 * Check if any unread data is available in Backup Storage,
 * @param[in] void
 * @return Bool
 *******************************************************************/
bool CBackupStorage::available(void)
{
    if(m_irPos != m_iwPos)
    { 
        return true;
    }
    return false;
}
/********************************************************************
 * store data in available file position in Storage,
 * @param[in] frame Data to be saved in file
 * @return File Error/FS error, length of data read from file
 *******************************************************************/
int CBackupStorage::readFromBS(FILESYSTEM *fileSystem, char *data)
{
    if(fileSystem->isMounted())
    {  
        char fname[20]= {0};
        int fLen = 0;
        sprintf(fname,"/BAK_%d.txt",m_irPos);
        debugPrintln(fname);
        fLen = fileSystem->readFile(fname,data); 
        return fLen;  
    }
    return FS_NOT_MOUNTED;
}
/********************************************************************
 * Move to next available file in Storage,
 * @param[in] void
 * @return void
 *******************************************************************/
int CBackupStorage::moveToNextFile(FILESYSTEM *fileSystem)
{
    if(fileSystem->isMounted())
    {  
        char fname[20]= {0};
        sprintf(fname,"/BAK_%d.txt",m_irPos);
        debugPrint("~~~~");
        debugPrintln(fname);
        fileSystem->writeFile(fname,"No data");
        m_irPos = (m_irPos + 1)% MAXFILES; 
    }
    return FS_NOT_MOUNTED;
}

/********************************************************************
 * Clear All file in Storage,
 * @param[in] void
 * @return void
 *******************************************************************/
int CBackupStorage::clearAllFiles(FILESYSTEM *fileSystem){
    if(fileSystem->isMounted())
    {  
        for(uint8_t filenm = 0;filenm < MAXFILES; filenm++)
        {
            char fname[20]= {0};
            sprintf(fname,"/BAK_%d.txt",filenm);
            debugPrint("~~~~");
            debugPrintln(fname);
            fileSystem->writeFile(fname,"No data");
        }
        m_irPos = 0;
        m_iwPos = 0;
    }
    return FS_NOT_MOUNTED;
}

int CBackupStorage::clearNonBackupFiles(FILESYSTEM *fileSystem) {
    if (!SPIFFS.begin(true)) {   // true = auto format if mount fails
        Serial.println("Failed to mount SPIFFS");
        return -1;
    }
    File root = SPIFFS.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open root directory");
        return -1;
    }
    File file = root.openNextFile();
    while (file) {
        String fname = String(file.name());
        if (fname.length() > 0) {
            String fname = String(file.name());
            fname.trim();   // remove whitespace

            if (!fname.startsWith("/")) {
                fname = "/" + fname;
            }
            // Remove only non-backup text files
            if (!fname.startsWith("/BAK_") && fname.endsWith(".txt")) {
                Serial.print("Removing: ");
                Serial.println(fname);
                if (SPIFFS.remove(fname)) {
                    Serial.println(" -> removed OK");
                } else {
                    Serial.println(" -> remove failed!");
                }
            }
        }
        file = root.openNextFile();
    }
    return 0; // success
}


/*Function to Count the no of files in the backup*/
int CBackupStorage::countStoredFiles(FILESYSTEM *fileSystem)
{
    int count = 0;
    if(fileSystem->isMounted())
    {
        for(int i = 0; i < MAXFILES; i++)
        {
            char fname[20] = {0};
            sprintf(fname, "/BAK_%d.txt", i);
            File file = SPIFFS.open(fname);//fileSystem->openFile(fname);
            
            if(file && !file.isDirectory() && file.size() > 10)  // Ensure it's not an empty placeholder
            {
                count++;
            }
        }
    }
    return count;
}




    
