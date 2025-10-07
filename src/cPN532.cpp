// Normal writing and reading a name

#include "cPN532.h"
#include "BSP.h"

Adafruit_PN532 nfc(-1, -1);

const uint8_t MIFARE_KEY_A[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Default key

/*construct*/
CPN532::CPN532() {}
/*destruct*/
CPN532::~CPN532() {}

void CPN532::init(void)
{
  nfc.begin();
  if (!nfc.getFirmwareVersion())
  {
    Serial.println("Didn't find PN532 board");
  }
  else
  {
    Serial.println("PN532 found!");
    Serial.println("Waiting for an NFC card...");
  }
  nfc.SAMConfig();
}
bool CPN532::writeDataToCard(const char *data)
{
  const uint8_t blocks[] = {8, 9, 10, 12}; // Skip block 11
  const uint8_t blockCount = sizeof(blocks) / sizeof(blocks[0]);
  uint8_t uid[7];
  uint8_t uidLength;

  Serial.println("Scan a card to write...");

  while (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100))
  {
    delay(100);
  }

  for (uint8_t i = 0; i < blockCount; i++)
  {
    uint8_t buffer[16];
    memset(buffer, 0, sizeof(buffer));
    strncpy((char *)buffer, data + (i * 16), 16);

    if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, blocks[i], 0x60, (uint8_t *)MIFARE_KEY_A))
    {
      Serial.print("Authentication failed for block ");
      Serial.println(blocks[i]);
      return FAIL;
    }

    if (!nfc.mifareclassic_WriteDataBlock(blocks[i], buffer))
    {
      Serial.print("Write failed for block ");
      Serial.println(blocks[i]);
      return FAIL;
    }
  }

  Serial.println("Write successful!");
  return SUCCESS;
}

uint8_t uid[7];
bool CPN532::isTagPresent()
{
  uint8_t uidLength;
  int i;
  i = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);
  return i;
}
bool CPN532::readDataFromCard(char *dest)
{
  const uint8_t blocks[] = {8, 9, 10, 12}; // Skip block 11
  const uint8_t blockCount = sizeof(blocks) / sizeof(blocks[0]);
  uint8_t uid[7];
  uint8_t uidLength;
  char *p = dest;

  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100))
  {
    return FAIL;
  }

  for (uint8_t i = 0; i < blockCount; i++)
  {
    uint8_t data[16];

    if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, blocks[i], 0x60, (uint8_t *)MIFARE_KEY_A))
    {
      Serial.print("Authentication failed for block ");
      Serial.println(blocks[i]);
      return FAIL;
    }

    if (!nfc.mifareclassic_ReadDataBlock(blocks[i], data))
    {
      Serial.print("Read failed for block ");
      Serial.println(blocks[i]);
      return FAIL;
    }

    memcpy(p, data, 16);
    p += 16;
  }

  dest[64] = '\0'; // Null-terminate the string
  Serial.print("Data read: ");
  Serial.println(dest);
  return SUCCESS;
}
