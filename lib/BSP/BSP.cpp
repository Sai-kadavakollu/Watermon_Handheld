#include <esp_task_wdt.h>
#include "BSP.h"
#include <SPI.h>

// #define BSP_SERIAL_DEBUG
#ifdef BSP_SERIAL_DEBUG
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

#define TOTAL_BUTTON 1
uint8_t PinNo[1] = {BSP_BTN_1};
uint8_t i2c_isBusy = 0;

/*************
 * Construct
 **************/
cBsp::cBsp()
{
}

/**************
 * Destruct
 ***************/
cBsp::~cBsp()
{
}

void cBsp::hooterInit()
{
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
}

void cBsp::hooterOn()
{
  digitalWrite(BUZZER_PIN, LOW);
}

void cBsp::hooterOff()
{
  digitalWrite(BUZZER_PIN, HIGH);
}

int BSP_IOread(int Pin)
{
  return digitalRead(Pin);
}

/*********************************************
 * Function to Intialize internal watchdog
 **********************************************/
void cBsp::wdtInit(void)
{
  if (pcbMake >= 2.0)
  {
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  }
}

/*********************************************
 * Function to add watchdog to the handler
 *********************************************/
void cBsp::wdtAdd(TaskHandle_t handle)
{
  if (pcbMake >= 2.0)
  {
    esp_task_wdt_add(handle); // add current thread to WDT watch
  }
}

/*********************************************
 * Function to feed the watchdog
 **********************************************/
void cBsp::wdtfeed(void)
{
  if (pcbMake >= 2.0)
  {
    esp_task_wdt_reset();
  }
}

/*************************************************************
 * indLedToggle() function is used to toggle the Indication
 * LED (Pin 2)
 * ***********************************************************/
void cBsp::indLedToggle(void)
{
  static bool st = false;
  st = !st;
  digitalWrite(BSP_LED_1, st);
}

/*************************************************************
 * IndLedOff() function is used to Drive Low the Indication
 * LED (Pin2)
 * ***********************************************************/
void cBsp::indLedOff(void)
{
  digitalWrite(BSP_LED_1, 0);
}
/**********************************************************
 * Function to logic state for the pin
 * pin -> I/O number to write the state
 * state -> logic state(1/0)
 *********************************************************/
void cBsp::ioPinWrite(uint8_t pin, bool state)
{
  digitalWrite(pin, state);
}

/**********************************************************
 * Function to read pin state
 * pin -> I/O number to read the state
 * Return the state of the pin(1/0)
 *********************************************************/
uint8_t cBsp::ioPinRead(uint8_t pin)
{
  return digitalRead(pin);
}

/********************************************************************
 * I2CInit() function is used to initialize the i2c communication
 * protocol and prints whether the I2C is initialized successfully
 * or failed.
 ********************************************************************/
void cBsp::i2cInitialization(void)
{
  /*BSP I2C GPIO PINS INITALIZATIONS*/
  if (Wire.begin(BSP_SDA, BSP_SCL))
  {
    debugPrintln("I2C Init Done...");
  }
  else
  {
    debugPrintln("I2C Init Error...");
  }
}
/*Write pin on board*/
void cBsp::BSP_IOwtire(int Pin, int st)
{
  digitalWrite(Pin, st);
}
/****************************************************************
 * SPIInit() function initializes the SPI communication protocol
 * and prints the debug message when initialization is done.
 * **************************************************************/
// void cBsp::spiInitialization(void)
// {
//   /*BSP SPI GPIO PINS INTIALIZATIONS*/
//   SPI.begin(BSP_CLK, BSP_MISO, BSP_MOSI, BSP_CS);
//   debugPrintln("SPI Init Done...");
// }

void cBsp::OnOffRelay(uint8_t relayNo, uint8_t state)
{
  ioPinWrite(relayNo, !state);
}

/*************************************************************
 * GPIO_INT() function is used to initialize various GPIO Pins
 * as input or outputs depending on the usage of pins.
 * ***********************************************************/
void cBsp::gpioInitialization(void)
{
  /*BSP LED PIN INITIALIZATION*/
  pinMode(BSP_LED_1, OUTPUT);
  hooterInit();
  /*BSP BUTTON PINS INITIALIZATION*/
  pinMode(BSP_BTN_1, INPUT_PULLUP);
  pinMode(BSP_PWR_DET, INPUT);
  debugPrintln("GPIO Initalized");
}

/*Button Pins*/
uint8_t KeyState[8] = {0};
/**
 * @brief  GetKay pressed on Keypad
 * @param  None
 * @retval None
 */

/*************************************************************
 * GetBtnEvent() function is used to get the current status of
 * 4 buttons and updates the array KeyState[] for each button
 * status.
 * ***********************************************************/
// uint8_t cBsp::getButtonEvent(void)
// {
//   uint8_t buttonSel = 0;
//   do
//   {
//     if (ioPinRead(BSP_BTN_1) == 0 && KeyState[buttonSel] == 0)
//     {
//       KeyState[buttonSel] = 1;
//       return buttonSel;
//     }
//     if (ioPinRead(BSP_BTN_1) == 1 && KeyState[buttonSel] == 1)
//     {
//       KeyState[buttonSel] = 0;
//     }
//     buttonSel++;
//   } while (buttonSel < TOTAL_BUTTON);
//   return 0xFF;
// }

/*************************************************************
 * wireWaitOnbusy() function is used to block the code untill
 * the I2C bus is ready.
 * RETRUN: Status of I2C bus
 *          true : if the I2C bus is free before Timeout
 *          false : if the I2C bus is not free before Timeout
 * ***********************************************************/
uint8_t wireBusy = 0;
/*Wait Till Wire is busy or TimeOut in ms*/
bool wireWaitOnbusy(void)
{
  int TimOut = 100;
  while (wireBusy && TimOut)
  {
    delay(1);
    TimOut--;
  }
  if (TimOut < 0)
  {
    return false;
  }
  return true;
}

/*************************************************************
 * EE_IO_Read() function is used to read data from EEPROM
 * ARGUMENTS:
 *          ssaddress: I2C address of EEPROM
 *          address : The memory address from where to read
 *          p_data  : Integer Pointer to store the data read from
 *                    EEPROM
 *          length  : No. of Bytes to fetch from the EEPROOM
 * RETURN:
 *          1 : EEPROM read is Successfull
 *          0 : EEPROM read error
 * ***********************************************************/
// int EE_IO_Read(uint16_t ssaddress, uint16_t address, uint8_t *p_data, uint8_t length)
// {
//   if(!wireWaitOnbusy()){
//     debugPrintln("EEOnbusy : I2c Read errror");
//     return false;
//   }

//   wireBusy = 1;
//   uint8_t nread = 0;
//   int ibyte;

//   uint8_t addr[2] = {(uint8_t)(address >> 8),(uint8_t)(address & 0xFF)};
//   delay(5);
//   Wire.beginTransmission(ssaddress);
//   size_t wret = Wire.write(addr,2);
//   if (Wire.endTransmission() != 0 ||
//       wret != 2)
//   {
//     wireBusy = 0;
//     debugPrintln("EE_IO_Read : I2c Read errror");
//     return 0;
//   }

//   Wire.requestFrom((uint8_t)ssaddress, length);

//   while (Wire.available() &&
//          ((ibyte = Wire.read()) != -1) &&
//          nread < length)
//   {
//     p_data[nread] = ibyte;
//     nread++;
//   }

//   wireBusy = 0;
//   return nread == length;
// }

/*************************************************************
 * EE_IO_Write() function is used to read data from EEPROM
 * ARGUMENTS:
 *          ssaddress: I2C address of EEPROM
 *          address : The memory address to write data into EEPROM
 *          p_data  : Integer Pointer which holds the data to write
 *                    into EEPROM
 *          length  : No. of Bytes to write to the EEPROOM
 * RETURN:
 *          1 : EEPROM write is Successfull
 *          0 : EEPROM write error
 * ***********************************************************/
// int EE_IO_Write(uint16_t ssaddress, uint16_t address, uint8_t * p_data, uint8_t length)
// {
//   if(!wireWaitOnbusy()){
//     debugPrintln("EEOnbusy : I2c write errror");
//     return false;
//   }
//   wireBusy = 1;
//   uint8_t addr[2] = {(uint8_t)(address >> 8),(uint8_t)(address & 0xFF)};
//   delay(5);
//   Wire.beginTransmission(ssaddress);
//   size_t wret = Wire.write(addr,2);
//   wret += Wire.write(p_data,length);
//    if ( Wire.endTransmission() != 0 ||
//       wret != (length+2))
//   {
//     debugPrintln("EE_IO_Write : I2c write errror");
//     wireBusy = 0;
//     return 0;
//   }
//   wireBusy = 0;
//   return 1;
// }

/*************************************************************
 * RTC_IO_Read() function is used to read data from EEPROM
 * ARGUMENTS:
 *          ssaddress: I2C address of RTC
 *          reg   : The memory address of register to write data
 *                into RTC
 *          bytes :  No. of Bytes to write to the RTC address
 *          in    :  Integer Pointer which holds the data to write
 *                   into RTC register
 * RETURN:
 *          TRUE  : RTC read was Successfull
 *          FALSE : RTC failed to Read
 * ***********************************************************/
bool RTC_IO_Read(uint16_t ssaddress, uint8_t reg, uint8_t bytes, uint8_t *in)
{
  uint8_t nread = 0;
  int ibyte;
  if (!wireWaitOnbusy())
  {
    debugPrintln("RTCOnbusy : I2c Read errror");
    return false;
  }

  delay(5);
  wireBusy = 1;
  Wire.beginTransmission(ssaddress);
  size_t wret = Wire.write(reg);
  if (Wire.endTransmission() != 0 ||
      wret != 1)
  {
    debugPrintln("RTC_IO_Read : I2c Read errror");
    wireBusy = 0;
    return false;
  }

  Wire.requestFrom((uint8_t)ssaddress, bytes);

  while (Wire.available() &&
         ((ibyte = Wire.read()) != -1) &&
         nread < bytes)
  {
    in[nread] = ibyte;
    nread++;
  }

  wireBusy = 0;
  return nread == bytes;
}

/*************************************************************
 * RTC_IO_Write() function is used to read data from EEPROM
 * ARGUMENTS:
 *          ssaddress: I2C address of RTC
 *          reg   : The memory address of register to read data
 *                  from RTC
 *          bytes :  No. of Bytes to read from the RTC address
 *          in    :  Integer Pointer to save the data read from
 *                   RTC register
 * RETURN:
 *          TRUE  : RTC write was Successfull
 *          FALSE : RTC failed to write
 * ***********************************************************/
bool RTC_IO_Write(uint16_t ssaddress, uint8_t reg, uint8_t bytes, uint8_t *out)
{
  if (!wireWaitOnbusy())
  {
    debugPrintln("RTCOnbusy : I2c write errror");
    return false;
  }
  delay(5);
  wireBusy = 1;
  Wire.beginTransmission(ssaddress);

  size_t wret = Wire.write(reg);
  wret += Wire.write(out, bytes);

  if (Wire.endTransmission() != 0 ||
      wret != (bytes + 1))
  {
    debugPrintln("RTC_IO_Write : I2c write errror");
    wireBusy = 0;
    return false;
  }

  wireBusy = 0;
  return true;
}

/*************************************************************
 * ConvertEpoch() function is used to take the RTC data and
 * convert into epoch time format.
 * ARGUMENT:
 *    PCF85063A* LoclRTC : The pointer of type PCF85063A
 * RETURN:
 *    Returns UNIX Time
 * ***********************************************************/
time_t cBsp:: ConvertEpoch(cPCF85063A *LoclRTC)
{
  if (LoclRTC->getTime() == 0)
  {
    // Serial.println("RTC Fail");
    return 0;
  }
  uint32_t y;
  uint32_t m;
  uint32_t d;
  uint32_t t;

  // Year
  y = (LoclRTC->m_iYear + 2000);
  // Month of year
  m = LoclRTC->m_iMonth;
  // Day of month
  d = LoclRTC->m_iDayOfMonth;
  // debugPrint(LoclRTC->hour); debugPrint(" : "); debugPrintln(LoclRTC->minute);
  // January and February are counted as months 13 and 14 of the previous year
  if (m <= 2)
  {
    m += 12;
    y -= 1;
  }
  // Convert years to days
  t = (365 * y) + (y / 4) - (y / 100) + (y / 400);
  // Convert months to days
  t += (30 * m) + (3 * (m + 1) / 5) + d;
  // Unix time starts on January 1st, 1970
  t -= 719561;
  // Convert days to seconds
  t *= 86400;
  // Add hours, minutes and seconds
  t += (3600 * LoclRTC->m_iHour) + (60 * LoclRTC->m_iMinute) + LoclRTC->m_iSecond;
  // Return Unix time
  // return (time_t)(t - 19800);
   return (time_t)(t);
}

/*************************************************************
 * SyncRTCTime() function is used to sync RTC module
 * ARGUMENT:
 *    PCF85063A* LoclRTC : The pointer of type PCF85063A
 * RETURN:
 *    0  : RTC moodule failed to respond or Failed to get CUrrent
 *         Time from Network
 *    1 : RTC module is in Sync
 * ***********************************************************/
int cBsp::syncRTCTime(cPCF85063A *LoclRTC, time_t serverEpoch, time_t RTCEpoch)
{
  tm timeinfo;

  if (serverEpoch < 1609439400) // 1609439400 is Friday, January 1, 2021 12:00:00 AM GMT+05:30
  {
    debugPrintln("Failed to obtain time,Wrong Year");
    return 0;
  }
  timeinfo = *gmtime(&serverEpoch);

  debugPrint("ServerEpoch: ");
  debugPrintln(serverEpoch);
  debugPrint("rtcEpoch: ");
  debugPrintln(RTCEpoch);
  /*If diff greater than 1 min sync RTC*/
  if (((RTCEpoch - serverEpoch) >= 60) || ((serverEpoch - RTCEpoch) >= 60))
  {

    LoclRTC->fillByYMD(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday); // Jan 19,2013
    LoclRTC->fillByHMS(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);             // 15:28 30"
    LoclRTC->fillDayOfWeek(timeinfo.tm_wday);                                           // Saturday
    if (LoclRTC->setTime() == 0)
    {
      debugPrintln("RTC set Time Failed.....");
      return 0;
    }
    m_bIsRtcSynced = 1;
    debugPrintln("RTC Time Synced.....");
  }
  else
  {
    debugPrintln("RTC In Sync.....");
  }
  return 1;
}
