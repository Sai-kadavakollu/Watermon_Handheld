#ifndef PN532_H
#define PN532_H

#include <Adafruit_PN532.h>

#define SUCCESS 1
#define FAIL 0

class CPN532
{
    public:
        CPN532(void);  // Constructor
        ~CPN532(void); // Destructor

        void init(void);                   // Initialize RFID module
        bool isTagPresent(void);        // Check if a new tag is present
        bool readDataFromCard(char *dest); // Read data from tag
        bool writeDataToCard(const char *data); // Write data to tag
    private:
        
};

#endif
