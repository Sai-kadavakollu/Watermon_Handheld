#ifndef TFT_H
#define TFT_H

#include <TFT_eSPI.h>
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include "CPondConfig.h"
#include "company_logo_220x220.h"

/*POPPINS FAMILY*/
#include "POPPINS_SEMIBOLD_09pt7b.h"
#include "POPPINS_SEMIBOLD_012pt7b.h"
#include "POPPINS_SEMIBOLD_016pt7b.h"
#include "POPPINS_SEMIBOLD_020pt7b.h"
#include "POPPINS_SEMIBOLD_028pt7b.h"
#include "POPPINS_SEMIBOLD_034pt7b.h"

/*CALIBRI FAMILY*/
#include "calibri_regular5pt7b.h"
#include "calibri_regular6pt7b.h"
#include "calibri_regular7pt7b.h"
#include "calibri_regular8pt7b.h"
#include "calibri_regular10pt7b.h"
#include "calibri_regular12pt7b.h"
#include "calibri_regular14pt7b.h"
#include "calibri_regular16pt7b.h"
#include "calibri_regular20pt7b.h"

/*POP-UP FRAME STATUS*/
#define NO_FRAME_IN_PROCESS 0
#define FRAME_UPLOAD_SUCCESS 1
#define FRAME_UPLOAD_FAIL 2
#define BACKUP_FRAME_UPLOAD_SUCCESS 3
#define BACKUP_FRAME_UPLOAD_FAIL 4
#define FRAME_UPLOAD_FAIL_NO_INTERNET 5
#define FRAME_GEN_FAILED_NO_GPS 6
#define FRAME_GEN_FAILED 7
#define FRAME_CAPTURE_COUNTDOWN 8

/*TFT Display Dimensions — LANDSCAPE*/
// #define SCREEN_WIDTH  320   // was 240 (landscape)
// #define SCREEN_HEIGHT 240   // was 320 (landscape)
// #define OREINTATION 3

/*TFT Display Dimensions — PORTRAIT*/
#define SCREEN_WIDTH 240  // was 240 (portrait)
#define SCREEN_HEIGHT 320 // was 320 (portrait)
#define OREINTATION 0

#define TIMER_COUNTDOWN 60

#define DARK_BG TFT_BLACK
#define DARK_FG TFT_WHITE
#define LIGHT_BG TFT_WHITE
#define LIGHT_FG TFT_BLACK

#define LOGO_COLOUR 0x8A2BE2

typedef struct __attribute__((packed))
{
  char pName[20];
  char nearestPonds[40] = "No ponds in range";
  float DoValueMgL;
  float DoSaturationValue;
  float TempValue;
  float Salinity;
} leftPanel_t;

typedef struct __attribute__((packed))
{
  char time[10];
  uint8_t Satellites;
  int rssi;
  int batteryPercentage;
  bool LocationStatus;
  bool FramesInBackup;
} Header_t;

typedef struct __attribute__((packed))
{
  bool isWebScoketsConnected = false;
  bool isHttpConnected = false;
  char ServerIp[25] = "0.0.0.0";
  char LocalIp[25] = "0.0.0.0";
  char RouterMac[25] = "No Internet";
  char DeviceMac[25] = "";
  uint8_t FooterType = 0;
} Footer_t;

enum ButtonEvent
{
  BUTTON_NONE,
  JUST_PRESSED,
  SHORT_PRESS_DETECTED,
  LONG_PRESS_DETECTED
};

typedef struct __attribute__((packed))
{
  uint8_t UploadStatus;
  char pName[20];
  char time[10];
  float doValue;
} PopUp_t;

typedef struct __attribute__((packed))
{
  char WiFiSsid[50];
  char WiFiPass[30];
  char FirmwareVersion[10];
  uint8_t backUpFramesCnt;
  ButtonEvent lastButtonEvent = BUTTON_NONE;
  uint8_t Counter;
  char DefaultServerIp[25] = "34.93.69.40";
  char DefaultWiFiSsid[50] = "Nextaqua_EAP110";
  bool IsSensorConnected = false;
} GeneralVaraibles_t;

enum ConfigState
{
  CONFIG_MENU,
  CONFIG_CONFIRM,
  CONFIG_EXECUTING,
  CONFIG_SUCCESS
};

class CDisplay
{
public:
  void begin(void);
  void ClearDisplay(void);
  // screentype: 1=Main, 2=Config (same as your code)
  void renderDisplay(uint8_t ScreenType, CPondConfig *PondConfig);
  void defaultDisplay(void);
  void DisplaySmartConfig(String Myname, String MyPassKey);
  void DisplaySaveSmartConfig(String NewSsid, String NewPassword);
  void printFOTA(int progress);
  void printSavingCoordinates(void);

  bool m_bSmartConfigMode = false;
  bool m_bResetServerFlag = false;
  bool m_bResetWifiFlag = false;
  bool StopReadingSensor = false;
  bool m_bSelectedReturnHome = false;
  bool m_bRefreshRightPanel = false;
  uint8_t m_bCalibrationResponse = 2;

  leftPanel_t DisplayLeftPanelData;
  Header_t DisplayHeaderData;
  Footer_t DisplayFooterData;
  GeneralVaraibles_t DisplayGeneralVariables;
  PopUp_t PopUpDisplayData;

private:
  std::map<std::string, PondInfo> lastPondStatusMap;

  leftPanel_t PrevDisplayLeftPanelData;
  Header_t PrevDisplayHeaderData;
  Footer_t PrevDisplayFooterData;

  ConfigState configState = CONFIG_MENU;
  bool calibrationSuccess = false;
  int currentSelection = 0; // 0 = YES, 1 = NO
  bool confirmYes = true;
  unsigned long executionStartTime = 0;
  int CounterToResetThePopuPScreens = 0;
  bool FirstTimeInRightPanel = false;

  /*LoadingPage -> WaterMon, Do sensor in water*/
  void LoadingPage();
  void MainDisplayHandler(CPondConfig *PondConfig);

  /*Icons in header*/
  void drawHeader();
  void drawSatelliteIcon(int x, int y);
  void drawCloud(int x, int y, int size, bool draw, uint16_t color, uint16_t bgColor);
  void DrawBattery(int X, int Y, int width, int height, int percentage, bool charging);
  void drawBatteryIcon(int x, int y, float batteryPercentage);
  void drawLocationSymbol(int x, int y);
  void drawWiFiSymbol(int x, int y, int rssi, uint16_t color);

  /*Data in the Footer -> LocalIP, ServerIP, Device MAC, Router MAC*/
  void drawFooter(uint8_t footerType);
  void FooterDebugMessages();
  void FooterPopUpMessages();
  void drawMemoryCard(int x, int y, int w, int h, uint16_t color);
  void drawTickInCircle(int x, int y, int radius);
  void drawXInCircle(int x, int y, int radius);
  void drawWifiQRCode(String ssid, String password);

  /*Data in the Left Panel -> PondName, NearestPondsData, DoValue, TempValue, SalintiyValue*/
  void drawLeftPanel();

  void drawRightPanel(CPondConfig *PondConfig);
  void drawRightPanel();
  void drawHourglass(int x, int y, int h, int countdown, int maxCountdown);
  void drawCircularTimer(int cx, int cy, int r, int countdown, int maxCountdown);
  void drawWrappedText(int x, int y, int maxWidth, const String &text, int lineGap = 1);

  /*Entering config mode,  Configuration Menu -> Reset Wifi, Reset Server, Calibrate, About Me, Return Home*/
  uint8_t configMenuIndex = 0;
  bool inConfirmDialog = false;
  bool confirmYesSelected = true;
  bool showResult = false;
  unsigned long resultTimer = 0;
  const uint8_t CONFIG_MENU_COUNT = 6;
  const int calibrationDuration = 180; // 180 seconds
  unsigned long calibrationStartMillis = 0;
  bool inCalibration = false;

  /*Default Pages*/
  void NextAqua(int x, int y, int b, int color);
  void drawCompanyLogo();

  void ConfigModeScreenHandler();
  void drawConfigMenuTemplate();
  void drawMenuText(uint8_t index);
  void drawConfirmDialog(uint8_t index);
  void drawYesNoOptions();
  void executeConfirmedAction(uint8_t index);
  void drawCalibrationStatus(int elapsedSec);

  /**/
  uint16_t bgColor();
  uint16_t fgColor();

  TFT_eSPI tft = TFT_eSPI();
};

#endif
