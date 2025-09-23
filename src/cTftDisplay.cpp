#include "cTftDisplay.h"

// #define DARK

#ifdef DARK
static inline uint16_t UI_BG() { return DARK_BG; }
static inline uint16_t UI_FG() { return DARK_FG; }
#else
static inline uint16_t UI_BG() { return LIGHT_BG; }
static inline uint16_t UI_FG() { return LIGHT_FG; }
#endif

/* Pond status colors (unchanged) */
const uint16_t pondStatusColors[] = {
  TFT_LIGHTGREY, // 0 - Not Active (Harvested)
#ifdef DARK
  TFT_BLACK,         // 1 - Yet to be taken
#else
  TFT_WHITE,         // 1 - Yet to be taken
#endif
  TFT_YELLOW,        // 2 - Taken but not uploaded
  TFT_GREEN,         // 3 - Taken and uploaded
  TFT_RED            // 4 - Error
};

// -------- Layout helpers (LANDSCAPE-AWARE) --------
static constexpr int HEADER_H  = 22;
static constexpr int FOOTER_H  = 83;

static inline int CONTENT_Y()  { return HEADER_H + 2; }
static inline int CONTENT_H()  { return SCREEN_HEIGHT - HEADER_H - FOOTER_H - 4; }

static constexpr int RIGHT_W   = 87;
static inline  int RIGHT_X()   { return SCREEN_WIDTH - RIGHT_W; }

static inline int SEP_X()      { return RIGHT_X() - 1; }
static inline int FOOTER_Y()   { return SCREEN_HEIGHT - FOOTER_H; }

// -------- Basic color helpers (keep names) --------
uint16_t CDisplay::bgColor() { return UI_BG(); }
uint16_t CDisplay::fgColor() { return UI_FG(); }


// -------- Lifecycle --------
void CDisplay::begin() {
  tft.init();
  tft.setRotation(OREINTATION);                 // LANDSCAPE
  tft.fillScreen(bgColor());
  tft.setTextColor(fgColor(), bgColor());
  LoadingPage();
}

void CDisplay::ClearDisplay(void) {
  tft.fillScreen(bgColor());
}

// -------- Top-level render --------
void CDisplay::renderDisplay(uint8_t ScreenType, CPondConfig *PondConfig) {
  if (m_bSelectedReturnHome) ScreenType = 1;

  switch (ScreenType) {
    case 1: MainDisplayHandler(PondConfig); break;
    case 2: ConfigModeScreenHandler(); break;
    default: break;
  }
}

// ---------- WORD WRAP HELPERS ----------
void CDisplay::drawWrappedText(int x, int y, int maxWidth, const String& text, int lineGap)
{
  // Assumes TL_DATUM; set your desired font before calling.
  const int fontH = tft.fontHeight();              // works with FreeFonts
  const int lh    = (fontH > 0 ? fontH - 2 : 10) + lineGap;

  String line;
  int cursorY = y;

  auto flushLine = [&](bool evenIfEmpty=false){
    if (line.length() || evenIfEmpty) {
      tft.drawString(line, x, cursorY);
      cursorY += lh;
      line = "";
    }
  };

  int start = 0, n = text.length();
  while (start < n) {
    // find next space (word)
    int sp = text.indexOf(' ', start);
    if (sp < 0) sp = n;
    String word = text.substring(start, sp);

    if (word.length() == 0) { // multiple spaces
      start = sp + 1;
      continue;
    }

    // if word itself is wider than maxWidth, split it char-by-char
    if (tft.textWidth(word) > maxWidth) {
      // place current line first
      flushLine(line.length() > 0);
      // chunk the long word
      String chunk;
      for (int i = 0; i < (int)word.length(); ++i) {
        String next = chunk + word[i];
        if (tft.textWidth(next) <= maxWidth) {
          chunk = next;
        } else {
          tft.drawString(chunk, x, cursorY);
          cursorY += lh;
          chunk = String(word[i]);
        }
      }
      // print last chunk
      if (chunk.length()) {
        tft.drawString(chunk, x, cursorY);
        cursorY += lh;
      }
      start = sp + 1;
      continue;
    }

    // normal word wrapping
    String test = (line.length() ? line + " " + word : word);
    if (tft.textWidth(test) <= maxWidth) {
      line = test;
    } else {
      flushLine(true);
      line = word;
    }

    start = sp + 1;
  }
  flushLine(); // last line
}

// =================== CONFIG MENU (unchanged logic, landscape-safe draws) ===================
void CDisplay::drawConfigMenuTemplate() {
  ClearDisplay();
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fgColor(), bgColor());
  tft.setFreeFont(&POPPINS_SEMIBOLD_012pt7b);
  tft.drawString("CONFIG MENU", SCREEN_WIDTH/2, 15);

  for (int i = 0; i < CONFIG_MENU_COUNT; i++) {
    tft.drawRoundRect(20, 40 + i * 40, SCREEN_WIDTH - 40, 35, 4, TFT_DARKGREY);
  }
}

void CDisplay::drawMenuText(uint8_t index) {
  const char *menuItems[CONFIG_MENU_COUNT] = {
    "Reset Server",
    "Reset WiFi",
    "Calibrate DO",
    "About Me",
    "Smart Config",
    "Return Home"
  };

  for (int i = 0; i < CONFIG_MENU_COUNT; i++) {
    if (i == index) {
      tft.fillRoundRect(20, 40 + i * 40, SCREEN_WIDTH - 40, 35, 4, TFT_SKYBLUE);
      tft.setTextColor(fgColor(), TFT_SKYBLUE);
    } else {
      tft.setTextColor(fgColor(), bgColor());
    }
    tft.setTextDatum(MC_DATUM);
    tft.drawString(menuItems[i], SCREEN_WIDTH/2, 55 + i * 40);
  }
}

void CDisplay::drawConfirmDialog(uint8_t index) {
  ClearDisplay();
  tft.setTextColor(0x000F, bgColor());
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&POPPINS_SEMIBOLD_016pt7b);
  switch (index) {
    case 0: tft.drawString("Reset Server", 12, 6); break;
    case 1: tft.drawString("Reset Wi-Fi", 12, 6); break;
    case 2: tft.setFreeFont(&POPPINS_SEMIBOLD_012pt7b);tft.drawString("DO Calibration", 12, 6); break;
    case 3: tft.drawString("About Me", 30, 6); break;
    case 4: tft.drawString("Smart Config", 12, 6); break;
    case 5: tft.drawString("Return Home", 12, 6); break;
    default: break;
  }
  tft.setTextColor(fgColor(), bgColor());
  tft.drawLine(0, 45, SCREEN_WIDTH, 45,fgColor());
    
  tft.setFreeFont(&calibri_regular10pt7b);
  switch (index) {
    case 0: { // Reset Server
      tft.setTextDatum(TL_DATUM);
      drawWrappedText(10, 50, SCREEN_WIDTH - 15, "Reset the server address to the default value.");
      tft.drawString("Default:", 10, 100);
      tft.drawString(DisplayGeneralVariables.DefaultServerIp, 75, 100);
      tft.drawString("Current:", 10, 125);
      tft.drawString(DisplayFooterData.ServerIp, 75, 125);
      tft.drawString("Hold YES for 3s to confirm", 10, 155);
      tft.drawString("Hold NO for 3s to cancel", 10, 180);
    } break;

    case 1: { // Reset Wi-Fi
      drawWrappedText(5, 50, SCREEN_WIDTH - 15, "Reset the Wi-Fi SSID to the default value.");
      tft.drawString("Default:", 5, 100);
      tft.drawString(DisplayGeneralVariables.DefaultWiFiSsid, 75, 100);
      tft.drawString("Current:", 5, 125);
      tft.drawString(DisplayGeneralVariables.WiFiSsid, 75, 125);
      tft.drawString("Hold YES for 3s to confirm", 5, 155);
      tft.drawString("Hold NO for 3s to cancel", 5, 180);
    } break;

    case 2: { // Calibrate DO
      if (!DisplayGeneralVariables.IsSensorConnected) {
        tft.drawString("Follow the below steps:", 10, 50);
        drawWrappedText(10, 80, SCREEN_WIDTH - 10, "1.Remove the sensor cap and expose it to air.");
        drawWrappedText(10, 125, SCREEN_WIDTH - 10, "2.Keep the device powered on.");
        tft.drawString("Hold YES for 3s to begin", 10, 170);
        tft.drawString("Hold NO for 3s to cancel", 10, 195);
      } else {
        tft.drawString("Sensor disconnected.", 10, 60);
        drawWrappedText(10, 90, SCREEN_WIDTH - 10, "Please Connect the sensor to calibrate.");
        tft.drawString("Hold YES for 3s to retry", 10, 155);
        tft.drawString("Hold NO for 3s to cancel", 10, 180);
      }
    } break;

    case 3:
      tft.setFreeFont(&calibri_regular10pt7b);
      tft.drawString("SSID:", 2, 50);
      tft.drawString(DisplayGeneralVariables.WiFiSsid, tft.textWidth("SSID:")+5, 50);
      tft.drawString("PASS:", 2, 90);
      tft.drawString(DisplayGeneralVariables.WiFiPass, tft.textWidth("PASS:")+5, 90);
      tft.drawString("ServerIP:", 2, 130);
      tft.drawString(DisplayFooterData.ServerIp, tft.textWidth("ServerIP:")+5, 130);
      tft.drawString("Firmware Version:", 2, 170);
      tft.drawString(DisplayGeneralVariables.FirmwareVersion, tft.textWidth("Firmware Version:")+5, 170);
      break;
    case 4: { // Smart Config
      tft.setFreeFont(&calibri_regular10pt7b);
      tft.setTextDatum(TL_DATUM);
      drawWrappedText(2, 60, SCREEN_WIDTH - 20, "Set new Wi-Fi credentials using Smart Config.");
      tft.drawString("Hold YES for 3s to continue", 2, 130);
      tft.drawString("Hold NO for 3s to cancel", 2, 160);
    } break;

    case 5: { // Return Home
      tft.drawString("Return to the Home screen.", 2, 60);
      tft.drawString("Hold YES for 3s to continue", 2, 120);
      tft.drawString("Hold NO for 3s to cancel", 2, 150);
    } break;
  }
  drawYesNoOptions();
}

void CDisplay::drawYesNoOptions() {
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);

  if (confirmYesSelected) {
    tft.fillRoundRect(30, 230, 70, 30, 5, TFT_GREEN);
    tft.setTextColor(fgColor(), TFT_GREEN);
    tft.drawString("YES", 65, 245);

    tft.fillRoundRect(SCREEN_WIDTH - 110, 230, 70, 30, 5, TFT_LIGHTGREY);
    tft.setTextColor(fgColor(), TFT_LIGHTGREY);
    tft.drawString("NO", SCREEN_WIDTH - 75, 245);
  } else {
    tft.fillRoundRect(30, 230, 70, 30, 5, TFT_LIGHTGREY);
    tft.setTextColor(fgColor(), TFT_LIGHTGREY);
    tft.drawString("YES", 65, 245);

    tft.fillRoundRect(SCREEN_WIDTH - 110, 230, 70, 30, 5, TFT_RED);
    tft.setTextColor(fgColor(), TFT_RED);
    tft.drawString("NO", SCREEN_WIDTH - 75, 245);
  }
}

bool inCalibration = false;
void CDisplay::ConfigModeScreenHandler() {
  ButtonEvent event = DisplayGeneralVariables.lastButtonEvent;
  DisplayGeneralVariables.lastButtonEvent = BUTTON_NONE;

  static unsigned long holdStartTime = 0;
  static bool waitingForHold = false;
  static bool firstTimeInConfig = false;

  if (!firstTimeInConfig) {
    ClearDisplay();
    drawConfigMenuTemplate();
    drawMenuText(configMenuIndex);
    firstTimeInConfig = true;
  }

  if (inCalibration) {
    int elapsed = (millis() - calibrationStartMillis) / 1000;
    if (elapsed > calibrationDuration) {
      StopReadingSensor = true;
      ClearDisplay();
      tft.setFreeFont(&calibri_regular12pt7b);
      tft.drawString(" Hi, Please Wait", 10, 50);
      tft.drawString(" Setting Cal Values", 10, 100);
      tft.drawString(" To the sensor", 10, 150);

      if (m_bCalibrationResponse != 2) {
        switch (m_bCalibrationResponse) {
          case 1:
            ClearDisplay();
            tft.setFreeFont(&calibri_regular14pt7b);
            tft.setTextColor(TFT_DARKGREEN);
            tft.drawString(" Sensor Calibration", 10, 50);
            tft.drawString(" Successful", 10, 100);
            tft.drawString(" I am ready", 10, 150);
            break;
          case 0:
            ClearDisplay();
            tft.setTextColor(TFT_RED);
            tft.setFreeFont(&calibri_regular14pt7b);
            tft.drawString(" Sensor Calibration", 10, 50);
            tft.drawString(" Failed", 10, 100);
            tft.drawString(" Please Re-Calibrate", 10, 150);
            break;
          default: break;
        }
        StopReadingSensor = false;
        inCalibration = false;
        showResult = true;
        resultTimer = millis();
      }
      return;
    } else {
      drawCalibrationStatus(elapsed);
      return;
    }
  }

  if (showResult) {
    if (millis() - resultTimer > 3000) {
      showResult = false;
      ClearDisplay();
      drawConfigMenuTemplate();
      drawMenuText(configMenuIndex);
    }
    return;
  }

  if (inConfirmDialog) {
    if (event == JUST_PRESSED) {
      confirmYesSelected = !confirmYesSelected;
      drawYesNoOptions();
    } else if (event == SHORT_PRESS_DETECTED) {
      if (confirmYesSelected) {
        waitingForHold = true;
        holdStartTime = millis();
      } else {
        inConfirmDialog = false;
        ClearDisplay();
        drawConfigMenuTemplate();
        drawMenuText(configMenuIndex);
      }
    }

    if (waitingForHold && millis() - holdStartTime >= 2000) {
      waitingForHold = false;
      executeConfirmedAction(configMenuIndex);
      inConfirmDialog = false;
    }
    return;
  }

  if (event == JUST_PRESSED) {
    configMenuIndex = (configMenuIndex + 1) % CONFIG_MENU_COUNT;
    drawConfigMenuTemplate();
    drawMenuText(configMenuIndex);
  } else if (event == SHORT_PRESS_DETECTED) {
    inConfirmDialog = true;
    confirmYesSelected = true;
    drawConfirmDialog(configMenuIndex);
  }
}

void CDisplay::drawCalibrationStatus(int elapsedSec) {
  // ClearDisplay();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fgColor(), bgColor());

  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  String header = "Calibrating(" + String(elapsedSec) + "/" + String(calibrationDuration) + "s)";
  tft.fillRect(tft.textWidth("Calibrating(")+2, 5, 120, 30, bgColor());
  tft.drawString(header, 2, 10);

  tft.setFreeFont(&calibri_regular10pt7b);
  tft.drawString("DO Sat (%):", 10, 60);
  tft.fillRect(tft.textWidth("DO Sat (%):")+15, 60, 20, 100, bgColor());
  tft.drawFloat(DisplayLeftPanelData.DoSaturationValue, 2,  tft.textWidth("DO Sat (%):")+15, 60);

  tft.drawString("DO (mg/L):", 10, 100);
  tft.fillRect(tft.textWidth("DO (mg/L):")+15, 100, 20, 100, bgColor());
  tft.drawFloat(DisplayLeftPanelData.DoValueMgL, 2, tft.textWidth("DO (mg/L):")+15, 100);

  tft.drawString("Temp (°C):", 10, 140);
  tft.fillRect(tft.textWidth("Temp (°C):")+15, 140, 20, 100, bgColor());
  tft.drawFloat(DisplayLeftPanelData.TempValue, 1, tft.textWidth("Temp (°C):")+15, 140);

  tft.setFreeFont(&calibri_regular10pt7b);
  tft.setTextDatum(TL_DATUM);
  int textAreaX = 15, textAreaW = SCREEN_WIDTH - 20;
  tft.fillCircle(7, 190, 3, TFT_RED);
  drawWrappedText(textAreaX, 185, textAreaW, "Please keep the device powered on.");
  tft.fillCircle(7, 245, 3, TFT_RED);
  drawWrappedText(textAreaX, 240, textAreaW, "Do not move the sensor, during calibration.");
}

void CDisplay::executeConfirmedAction(uint8_t index) {
  ClearDisplay();
  tft.setFreeFont(&POPPINS_SEMIBOLD_012pt7b);
  tft.setTextColor(TFT_DARKGREEN, bgColor());//green

  switch(index) {
    case 0:
      m_bResetServerFlag = true;
      tft.drawString("Server Reset Done", SCREEN_WIDTH/2, 100);
      break;
    case 1:
      m_bResetWifiFlag = true;
      tft.drawString("WiFi Reset Done", SCREEN_WIDTH/2, 100);
      break;
    case 2:
      calibrationStartMillis = millis();
      inCalibration = true;
      ClearDisplay();
      drawCalibrationStatus(0);
      return;
    case 3:
      tft.drawString("Returning back to", SCREEN_WIDTH/2, 100);
      tft.drawString(" Config Menu", SCREEN_WIDTH/2, 160);
      break;
    case 4:
      tft.drawString("Entering the", SCREEN_WIDTH/2, 100);
      tft.drawString("Smart Config Mode", SCREEN_WIDTH/2, 160);
      m_bSmartConfigMode = true;
      break;
    case 5:
      tft.drawString("Returning Home...", SCREEN_WIDTH/2, 100);
      ESP.restart();
      break;
  }

  showResult = true;
  resultTimer = millis();
}

// =================== MAIN DISPLAY ===================
void CDisplay::MainDisplayHandler(CPondConfig *PondConfig){
  if (memcmp(&PrevDisplayHeaderData, &DisplayHeaderData, sizeof(Header_t)) != 0) {
    drawHeader();
    memcpy(&PrevDisplayHeaderData, &DisplayHeaderData, sizeof(Header_t));
  }

  drawFooter(PopUpDisplayData.UploadStatus);

  if (memcmp(&PrevDisplayLeftPanelData, &DisplayLeftPanelData, sizeof(leftPanel_t)) != 0) {
    drawLeftPanel();
    memcpy(&PrevDisplayLeftPanelData, &DisplayLeftPanelData, sizeof(leftPanel_t));
  }

  if (PopUpDisplayData.UploadStatus == FRAME_CAPTURE_COUNTDOWN) {
    drawRightPanel();
  } else {
    drawRightPanel(PondConfig);
  }
}

// -------- Header --------
void CDisplay::drawHeader() {
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_H,  bgColor());
  tft.drawLine(0, HEADER_H + 1, SCREEN_WIDTH, HEADER_H + 1, fgColor());

  tft.setTextColor(fgColor());
  tft.setFreeFont(&calibri_regular10pt7b);
  tft.drawString(DisplayHeaderData.time, 5, 5);

  drawSatelliteIcon(95, 5);
  drawWiFiSymbol(125, 3, DisplayHeaderData.rssi, fgColor());

  tft.setTextColor(fgColor());
  tft.setFreeFont(&calibri_regular8pt7b);
  tft.drawString(String(DisplayHeaderData.rssi), (DisplayHeaderData.rssi == 0)?155:147, 7);

  drawLocationSymbol(100, 8);
  drawBatteryIcon(175, 9, DisplayHeaderData.batteryPercentage);

  tft.setFreeFont(&calibri_regular7pt7b);
  tft.setTextColor(fgColor());
  char batPercent[10];
  sprintf(batPercent, "%d%%", DisplayHeaderData.batteryPercentage);
  tft.drawString(batPercent, 210, 8);
}

void CDisplay::drawTickInCircle(int x, int y, int radius) {
  tft.fillCircle(x, y, radius, 0x07E0);
  tft.drawLine(x - radius / 3, y, x - 2, y + radius / 3, bgColor());
  tft.drawLine(x - 2, y + radius / 3, x + radius / 2, y - radius / 3, bgColor());
}

void CDisplay::drawXInCircle(int x, int y, int radius) {
  tft.fillCircle(x, y, radius, 0xF800);
  int offset = radius / 2;
  tft.drawLine(x - offset, y - offset, x + offset, y + offset, bgColor());
  tft.drawLine(x - offset, y + offset, x + offset, y - offset, bgColor());
}

void CDisplay::drawMemoryCard(int x, int y, int w, int h, uint16_t color) {
    uint16_t BodyColor = (DisplayGeneralVariables.backUpFramesCnt == 0) ? color: fgColor();
    uint16_t chipsColor = (DisplayGeneralVariables.backUpFramesCnt == 0) ? fgColor() : TFT_LIGHTGREY;
  int notchH = h / 4;
  int cornerCut = w / 5;

  // Main rectangle (lower part)
  tft.fillRect(x, y + notchH, w, h - notchH, BodyColor);

  // Top cut corner (triangle)
  tft.fillTriangle(x + w - cornerCut, y, x + w, y, x + w, y + notchH, BodyColor);

  // Top flat part
  tft.fillRect(x, y, w - cornerCut, notchH, BodyColor);

  // Gold pins
  int pinW = w / 8;
  int pinH = notchH - 2;
  int pinSpacing = pinW + 2;
  int startX = x + 3;
  int pinY = y + 2;
  for (int i = 0; i < 4; i++) {
    tft.fillRect(startX + i * pinSpacing, pinY, pinW, pinH, chipsColor);
  }
}

// -------- Footer + popups (PRO text) --------
void CDisplay::drawFooter(uint8_t footerType) {
  static uint8_t prevUploadStatus = (uint8_t)-1;

  if (PopUpDisplayData.UploadStatus != NO_FRAME_IN_PROCESS &&
      PopUpDisplayData.UploadStatus != FRAME_CAPTURE_COUNTDOWN) {
    CounterToResetThePopuPScreens++;
    if (CounterToResetThePopuPScreens >= 20) {
      PopUpDisplayData.UploadStatus = NO_FRAME_IN_PROCESS;
      CounterToResetThePopuPScreens = 0;
    }
  } else if (PopUpDisplayData.UploadStatus == FRAME_CAPTURE_COUNTDOWN) {
    // Countdown mode tips
    tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
    tft.setFreeFont(&calibri_regular10pt7b);

    // drawHourglass(SCREEN_WIDTH - 135, FOOTER_Y() + 31, 35,
    //               TIMER_COUNTDOWN - DisplayGeneralVariables.Counter, TIMER_COUNTDOWN);

    tft.setCursor(5, FOOTER_Y() + 22); tft.print("Immerse the DO sensor in");
    tft.setCursor(5, FOOTER_Y() + 44); tft.print("the pond and please....");
    tft.setCursor(5, FOOTER_Y() + 66); tft.print("Wait until the timer is 0");
  } else {
    // Normal debug footer
    tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
    FooterDebugMessages();
  }

  if (prevUploadStatus != PopUpDisplayData.UploadStatus) {
    char dispBuff[48];
    snprintf(dispBuff, sizeof(dispBuff), "%s|%s|%.2fmg/l",
             PopUpDisplayData.time, PopUpDisplayData.pName, PopUpDisplayData.doValue);

    switch (PopUpDisplayData.UploadStatus) {
      case BACKUP_FRAME_UPLOAD_SUCCESS:
        tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
        drawTickInCircle(18, 290, 15);
        tft.setFreeFont(&calibri_regular12pt7b);
        tft.setCursor(10, 265); tft.print("Backup upload:Success");
        tft.setCursor(35, 295); tft.print(dispBuff);
        break;

      case BACKUP_FRAME_UPLOAD_FAIL:
        tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
        drawXInCircle(18, 290, 15);
        tft.setFreeFont(&calibri_regular10pt7b);
        tft.setCursor(10, 265); tft.print("Backup upload:Failed");
        tft.setCursor(35, 295); tft.print(dispBuff);
        break;

      case FRAME_UPLOAD_SUCCESS:
        tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
        drawTickInCircle(18, 290, 15);
        tft.setFreeFont(&calibri_regular12pt7b);
        tft.setCursor(10, 265); tft.print("Frame upload:Success");
        tft.setCursor(35, 295); tft.print(dispBuff);
        break;

      case FRAME_UPLOAD_FAIL:
        tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
        drawXInCircle(18, 290, 15);
        tft.setFreeFont(&calibri_regular12pt7b);
        tft.setCursor(10, 265); tft.print("Frame upload:Failed");
        tft.setCursor(35, 295); tft.print(dispBuff);
        break;

      case FRAME_UPLOAD_FAIL_NO_INTERNET:
        tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
        drawMemoryCard(10, 280, 20, 23, TFT_DARKGREY);
        tft.setFreeFont(&calibri_regular10pt7b);
        tft.setTextDatum(TL_DATUM);
        tft.setCursor(10, 265); tft.print("Saved to local backup");
        tft.setCursor(35, 300);tft.print(dispBuff);
        break;

      case FRAME_GEN_FAILED_NO_GPS:
        tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
        drawXInCircle(18, 290, 15);
        tft.setFreeFont(&calibri_regular10pt7b);
        tft.setCursor(10, 265); tft.print("Frame generation failed");
        tft.setCursor(35, 295); tft.print("GPS fix unavailable");
        break;

      case FRAME_GEN_FAILED:
        tft.fillRect(0, FOOTER_Y(), SCREEN_WIDTH, FOOTER_H, bgColor());
        tft.setFreeFont(&calibri_regular12pt7b);
        tft.setCursor(5, FOOTER_Y() + 16); tft.print("Frame generation failed");
        tft.setCursor(5, FOOTER_Y() + 46); tft.print("Clock not synchronized");
        tft.setCursor(5, FOOTER_Y() + 66); tft.print("Verify date/time and retry");
        break;

      default: break;
    }
    prevUploadStatus = PopUpDisplayData.UploadStatus;
  }
}

// -------- Footer debug block (labels/size tuned) --------
void CDisplay::FooterDebugMessages(){
  int y0 = FOOTER_Y() - 1;
  tft.drawLine(0, y0, SCREEN_WIDTH, y0, fgColor());
  tft.drawLine(0, y0 + 1, SCREEN_WIDTH, y0 + 1, fgColor());

  tft.setTextColor(fgColor());
  tft.setFreeFont(&calibri_regular8pt7b);

  // Column 1
  int col1x = 2;
  int rowY  = FOOTER_Y() + 10;
  tft.fillCircle(10, rowY, 5, (DisplayFooterData.isHttpConnected)? TFT_GREEN : TFT_RED);
  tft.setCursor(20, rowY + 5);  tft.print("Local IP");
  tft.setCursor(col1x, rowY + 25); tft.print(WiFi.localIP());

  int rowY2 = FOOTER_Y() + 50;
  tft.fillCircle(10, rowY2, 5, (DisplayFooterData.isWebScoketsConnected)? TFT_GREEN : TFT_RED);
  tft.setCursor(20, rowY2 + 5); tft.print("Server IP");
  tft.setCursor(col1x, rowY2 + 25); tft.print(DisplayFooterData.ServerIp);

  // Column 2
  int col2x = SCREEN_WIDTH/2 - 5;
  tft.setCursor(col2x, rowY + 5);   tft.print("Device MAC");
  tft.setCursor(col2x, rowY + 25);  tft.print(DisplayFooterData.DeviceMac);
  tft.setCursor(col2x, rowY2 + 5);  tft.print("Router MAC");
  tft.setCursor(col2x, rowY2 + 25);
  if (strcmp(DisplayFooterData.RouterMac, "")) tft.print(DisplayFooterData.RouterMac);
  else tft.print("No Internet");
}

// -------- Header icons --------
void CDisplay::drawLocationSymbol(int x, int y) {
  uint16_t useColor = DisplayHeaderData.LocationStatus ? fgColor() : TFT_LIGHTGREY;
  tft.fillCircle(x, y, 5, useColor);
  tft.fillCircle(x, y, 2, bgColor());
  tft.fillTriangle(x - 4, y + 3, x + 4, y + 3, x, y + 11, useColor);
  tft.setCursor(140, HEADER_H + 1);
  tft.setFreeFont(&calibri_regular10pt7b);
  tft.setTextColor(useColor);
}

void CDisplay::drawWiFiSymbol(int x, int y, int rssi, uint16_t color) {
  int barWidth = 3;
  tft.fillRect(x + 21, y + 5, barWidth, 1, color);
  tft.fillRect(x + 9,  y + 5, barWidth, 1, color);
  tft.fillRect(x + 19, y + 4, barWidth, 1, color);
  tft.fillRect(x + 11, y + 4, barWidth, 1, color);
  tft.fillRect(x + 17, y + 3, barWidth, 1, color);
  tft.fillRect(x + 13, y + 3, barWidth, 1, color);
  tft.fillRect(x + 16, y + 3, barWidth, 1, color);
  tft.fillRect(x + 14, y + 3, barWidth, 1, color);

  tft.fillRect(x + 19, y + 8, barWidth, 1, color);
  tft.fillRect(x + 11, y + 8, barWidth, 1, color);
  tft.fillRect(x + 17, y + 7, barWidth, 1, color);
  tft.fillRect(x + 13, y + 7, barWidth, 1, color);
  tft.fillRect(x + 15, y + 6, barWidth, 1, color);

  tft.fillRect(x + 17, y + 11, barWidth, 1, color);
  tft.fillRect(x + 13, y + 11, barWidth, 1, color);
  tft.fillRect(x + 15, y + 10, barWidth, 1, color);

  tft.fillRect(x + 15, y + 13, barWidth, 1, color);

  if (DisplayHeaderData.rssi == 0) {
    tft.setCursor(x+18, y+18);
    tft.setTextColor(TFT_RED);
    tft.setFreeFont(&calibri_regular7pt7b);
    tft.print("X");
  } else {
    tft.setCursor(x+18, y+18);
    tft.setTextColor(bgColor());
    tft.setFreeFont(&calibri_regular7pt7b);
    tft.print("X");
  }
}

void CDisplay::drawBatteryIcon(int x, int y, float batteryPercentage) {
  static float lastBatteryPercentage = -1;
  lastBatteryPercentage = batteryPercentage;

  int width = 30;
  int height = 12;
  int barPadding = 2;
  int barWidth = width - 4;
  int barHeight = height - 4;

  batteryPercentage = constrain(batteryPercentage, 0, 100);

  tft.fillRect(x + 2, y + 2, barWidth, barHeight, bgColor());

  tft.drawRect(x, y, width, height, fgColor());
  tft.fillRect(x + width, y + (height / 4), 3, height / 2, fgColor());

  uint16_t barColor;
  if (batteryPercentage > 50)      barColor = tft.color565(0, 255, 0);
  else if (batteryPercentage > 30) barColor = tft.color565(255, 255, 0);
  else                             barColor = tft.color565(255, 0, 0);

  int fillWidth = ::map(batteryPercentage, 0, 100, 0, barWidth);
  tft.fillRect(x + barPadding, y + barPadding, fillWidth, barHeight, barColor);
}

void CDisplay::DrawBattery(int X, int Y, int width, int height, int percentage, bool charging) {
  percentage = constrain(percentage, 0, 100);
  int capWidth = width / 3;
  int capHeight = height / 8;
  int fillHeight = ::map(percentage, 0, 100, 0, height - 4);

  uint16_t color;
  if (percentage >= 60) color = TFT_GREEN;
  else if (percentage >= 30) color = TFT_ORANGE;
  else color = TFT_RED;

  static bool blinkState = true;
  if (percentage < 10) {
    blinkState = !blinkState;
    if (!blinkState) color = TFT_WHITE;
  }

  tft.fillRect(X - 1, Y - capHeight - 1, width + 2, height + capHeight + 2, TFT_WHITE);
  tft.drawRect(X, Y, width, height, TFT_BLACK);
  tft.fillRect(X + (width - capWidth) / 2, Y - capHeight, capWidth, capHeight, TFT_BLACK);
  tft.fillRect(X + 2, Y + (height - fillHeight - 2), width - 4, fillHeight, color);

  if (charging) {
    int cx = X + width / 2;
    int cy = Y + height / 2;
    int size = width / 2;
    int x1 = cx - size / 2, y1 = cy - size;
    int x2 = cx + size / 2, y2 = cy - size / 3;
    int x3 = cx - size / 6, y3 = cy + size / 3;
    int x4 = cx + size / 3, y4 = cy + size;
    tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
    tft.fillTriangle(x3, y3, x2, y2, x4, y4, TFT_YELLOW);
  }

  tft.fillRect(X + width + 1, Y, 23, 20, TFT_WHITE);
  tft.setFreeFont(&calibri_regular5pt7b);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(X + width + 1, Y + 8);
  tft.print(percentage); tft.print("%");
}

/* Cloud icon (used for websocket indication) */
void CDisplay::drawCloud(int x, int y, int size, bool draw, uint16_t color, uint16_t bg) {
  int r = size;
  uint16_t useColor = draw ? color : bg;
  tft.fillCircle(x + r, y + r , r*0.9, useColor);
  tft.fillCircle(x + 2.5 * r, y , r * 1.2, useColor);
  tft.fillCircle(x + 4 * r, y + r, r * 0.9, useColor);
  tft.fillRoundRect(x + r, y + r, 3.5 * r, r * 1.2, r * 0.6, useColor);
}

void CDisplay::NextAqua(int x, int y, int b, int color) {
  tft.setTextColor(fgColor());
  tft.setFreeFont(&POPPINS_SEMIBOLD_016pt7b);
  tft.setTextSize(0);
  tft.setCursor(25, 50);
  tft.println("NEXTAQUA");
  tft.fillRect(x, y, b, b, color);
  tft.fillRect(x, y + (2 * b), b, b, color);
  tft.fillRect(x, y + (4 * b), b, b, color);
  tft.fillRect(x, y + (6 * b), b, b, color);
  tft.fillRect(x, y + (8 * b), b, b, color);
  tft.fillRect(x + (2 * b), y + (2 * b), b, b, color);
  tft.fillRect(x + (2 * b), y + (4 * b), b, b, color);
  tft.fillRect(x + (2 * b), y + (6 * b), b, b, color);
  tft.fillRect(x + (4 * b), y + (4 * b), b, b, color);
}

void CDisplay::defaultDisplay(){
  tft.setTextColor(0x22fa);
  tft.setFreeFont(&POPPINS_SEMIBOLD_016pt7b);
  tft.setCursor(2, 50);
  tft.println("AquaExchange");
  tft.setTextColor(fgColor());
  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  tft.setCursor(2, 100); tft.print("MacId:");
  tft.setFreeFont(&calibri_regular10pt7b); tft.println(DisplayFooterData.DeviceMac);

  tft.setCursor(2, 140);
  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b); tft.print("SSID:");
  tft.setFreeFont(&calibri_regular10pt7b); tft.print(DisplayGeneralVariables.WiFiSsid);

  tft.setCursor(2, 180);
  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b); tft.print("ServerIP:");
  tft.setFreeFont(&calibri_regular10pt7b); tft.print(DisplayFooterData.ServerIp);

  tft.setCursor(2, 220);
  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b); tft.print("Firmware Version:");
  tft.setFreeFont(&calibri_regular10pt7b); tft.print(DisplayGeneralVariables.FirmwareVersion);
}

// -------- Left panel --------
void CDisplay::drawLeftPanel() {
  tft.fillRect(0, CONTENT_Y(), SEP_X(), CONTENT_H(),  bgColor());
  tft.setTextColor(fgColor());

  tft.setFreeFont(&POPPINS_SEMIBOLD_012pt7b);
  char PondName[20];
  sprintf(PondName, "Pond:%s", DisplayLeftPanelData.pName);
  if(strcmp(PondName, "Pond:") == 0){
    tft.drawString("Pond: --", 5, CONTENT_Y() + 9);
  } else {
    tft.drawString(PondName, 5, CONTENT_Y() + 9);
  }

  tft.setFreeFont(&calibri_regular8pt7b);
  // tft.drawString(DisplayLeftPanelData.nearestPonds, 2, CONTENT_Y() + 44);
  drawWrappedText(2, CONTENT_Y() + 38, 150, DisplayLeftPanelData.nearestPonds);

  tft.setFreeFont(&POPPINS_SEMIBOLD_028pt7b);
  tft.drawFloat(DisplayLeftPanelData.DoValueMgL, 2, 3, CONTENT_Y() + 64);
  tft.setFreeFont(&calibri_regular12pt7b);
  tft.drawString("mg/L", 90, CONTENT_Y() + 114);

  tft.setFreeFont(&POPPINS_SEMIBOLD_016pt7b);
  String t = String(DisplayLeftPanelData.TempValue, 1);
  int ty = CONTENT_Y() + 139;
  tft.drawString(t, 5, ty);
  int TempWidth = tft.textWidth(t);
  tft.setFreeFont(&calibri_regular14pt7b);
  tft.drawString("c", TempWidth+10, ty + 10);

  tft.setFreeFont(&POPPINS_SEMIBOLD_016pt7b);
  String salStr = String(DisplayLeftPanelData.Salinity, 1);
  int sy = CONTENT_Y() + 174;
  tft.drawString(salStr, 5, sy);
  int salWidth = tft.textWidth(salStr);
  tft.setFreeFont(&calibri_regular12pt7b);
  tft.drawString("ppt", salWidth+10, sy + 10);

  tft.drawLine(SEP_X(), HEADER_H + 2, SEP_X(), HEADER_H + 2 + CONTENT_H(), fgColor());//separator
}

// -------- Right panel (timer) --------
void CDisplay::drawRightPanel() {
  int value = TIMER_COUNTDOWN - DisplayGeneralVariables.Counter;
  tft.fillRect(RIGHT_X(), CONTENT_Y(), RIGHT_W, CONTENT_H(), bgColor());
  drawCircularTimer(RIGHT_X() + RIGHT_W/2 , CONTENT_Y() + CONTENT_H()/2, 35, value, TIMER_COUNTDOWN);
  FirstTimeInRightPanel = true;
}

void CDisplay::drawHourglass(int x, int y, int h, int countdown, int maxCountdown) {
  int topHeight = (h/2) * countdown / maxCountdown;
  int bottomHeight = (h/2) - topHeight;

  uint16_t sandColor  = tft.color565(205, 133, 63);
  uint16_t woodColor  = tft.color565(139, 69, 19);
  uint16_t frameColor = TFT_BLACK;

  int cx = x + h/2;   // center X

  tft.fillRoundRect(x-5, y-8, h+10, 6, 3, woodColor);
  tft.fillRoundRect(x-5, y+h+2, h+10, 6, 3, woodColor);

  tft.fillRect(x+1, y+1, h-2, h-2, bgColor());

  if (topHeight > 0) {
    tft.fillTriangle(x, y, x+h, y, cx, y+topHeight, sandColor);
  }
  if (bottomHeight > 0) {
    tft.fillTriangle(x, y+h, x+h, y+h, cx, y+h-bottomHeight, sandColor);
  }
  if (countdown > 0) {
    tft.drawLine(cx, y+topHeight, cx, y+h-bottomHeight, sandColor);
  }
}

void CDisplay::drawCircularTimer(int cx, int cy, int r, int countdown, int maxCountdown) {
  const int thickness = 2;
  const int half      = thickness / 2;

  float anglePerStep = 360.0f / (float)maxCountdown;
  float endAngle     = countdown * anglePerStep;

  uint16_t ringBgColor = TFT_BROWN;
  uint16_t arcColor    = TFT_GREEN;

  for (int rr = r - half; rr <= r + half; ++rr) {
    tft.drawCircle(cx, cy, rr, ringBgColor);
  }
  for (int deg = 0; deg < (int)endAngle; ++deg) {
    float rad = (deg - 90) * 0.0174532925f;
    float cs = cosf(rad), sn = sinf(rad);
    for (int rr = r - half; rr <= r + half; ++rr) {
      int x = cx + (int)(cs * rr);
      int y = cy + (int)(sn * rr);
      tft.drawPixel(x, y, arcColor);
    }
  }
  tft.setTextColor(fgColor(), bgColor());
  tft.setFreeFont(&POPPINS_SEMIBOLD_020pt7b);
  tft.setCursor((countdown < 10) ? cx - 12 : cx - 23, cy + 13);
  tft.print(countdown);
}

void CDisplay::drawSatelliteIcon(int x, int y) {
  uint16_t color = (DisplayHeaderData.Satellites > 0) ? fgColor() : TFT_LIGHTGREY;
  tft.setTextColor(color);
  tft.drawString(String(DisplayHeaderData.Satellites), x+15, y);
}

void CDisplay::drawRightPanel(CPondConfig *PondConfig) {
  if (!FirstTimeInRightPanel) {
    if (PondConfig->m_pondStatusMap == lastPondStatusMap) {
      return; // No change, skip redraw
    }
  }
  FirstTimeInRightPanel = false;

  // Clear panel area
  tft.fillRect(153, 25, 82, 208, bgColor());

  // Save a copy of the new state
  lastPondStatusMap = PondConfig->m_pondStatusMap;

  // Group ponds by section letter (A, B, C…)
  // std::map<char, std::vector<int>> pondGroups;
  // for (const auto& pair : PondConfig->m_pondStatusMap) {
  //   const std::string& key = pair.first;
  //   if (key.length() < 2) continue;

  //   char section = key[0];
  //   int number = atoi(key.substr(1).c_str());
  //   pondGroups[section].push_back(number);
  // }
    std::map<std::string, std::vector<int>> pondGroups;
    for (const auto& pair : PondConfig->m_pondStatusMap) {
      const std::string& key = pair.first;
      if (key.empty()) continue;

      // Find where digits start
      size_t pos = 0;
      while (pos < key.size() && !isdigit(key[pos])) {
          pos++;
      }

      if (pos == 0 || pos >= key.size()) continue; // invalid format

      std::string section = key.substr(0, pos);   // e.g. "PS"
      int number = atoi(key.substr(pos).c_str()); // e.g. 1 or 10

      pondGroups[section].push_back(number);
    }


  // Layout settings
  int boxSize = 16;
  int marginX = 4;
  int marginY = 3;
  int startX = 155;   // first column start
  int startY = 45;
  int currentColX = startX;
  int maxRows = 10;

  tft.setFreeFont(&calibri_regular5pt7b);

  bool firstSection = true;
  for (const auto& group : pondGroups) {
    const std::string& section = group.first;
    std::vector<int> sortedNums = group.second;
    std::sort(sortedNums.begin(), sortedNums.end());

    int pondCount = sortedNums.size();

    // Draw section header
    tft.setFreeFont(&calibri_regular10pt7b);
    tft.setCursor(currentColX + 2, startY - 6);
    tft.print(section.c_str());

    tft.drawLine(152, startY - 2, 240, startY - 2, fgColor());

    // Separator line (except before the first section)
    if (!firstSection) {
      tft.drawLine(currentColX - (marginX / 2), startY - 21,
                   currentColX - (marginX / 2), startY + maxRows * (boxSize + marginY),
                   fgColor());
    }
    firstSection = false;

    // Draw ponds
    tft.setFreeFont(&calibri_regular6pt7b);

    if (pondCount <= 10) {
      // Single column
      for (int i = 0; i < pondCount && i < maxRows; i++) {
        int x = currentColX;
        int y = startY + i * (boxSize + marginY);
        int boxNum = sortedNums[i];

        char pondKey[10];
        snprintf(pondKey, sizeof(pondKey), "%s%d", section.c_str(), boxNum);

        uint8_t status = 0;
        auto it = PondConfig->m_pondStatusMap.find(pondKey);
        if (it != PondConfig->m_pondStatusMap.end()) {
          status = it->second.Backup;
        }

        uint16_t fillColor = pondStatusColors[status];
        tft.fillRoundRect(x, y, boxSize, boxSize, 3, fillColor);
        tft.drawRoundRect(x, y, boxSize, boxSize, 3, fgColor());

        int textX = (boxNum < 10) ? (x + 5) : (x + 2);
        tft.setCursor(textX, y + 10);
        tft.setTextColor(fgColor());
        tft.print(boxNum);
      }
      currentColX += (boxSize + marginX); // move to next column
    } else {
      // Multi-column alternating (10 per column)
      int colsNeeded = (pondCount + maxRows - 1) / maxRows; // ceil(pondCount/10)

      for (int col = 0; col < colsNeeded; col++) {
        int startIdx = col * maxRows;
        int endIdx   = std::min(startIdx + maxRows, pondCount);

        std::vector<int> colNums(sortedNums.begin() + startIdx, sortedNums.begin() + endIdx);

        // Reverse order for odd columns (2nd, 4th, …)
        if (col % 2 == 1) {
          std::reverse(colNums.begin(), colNums.end());
        }

        for (int i = 0; i < (int)colNums.size(); i++) {
          int boxNum = colNums[i];
          int x = currentColX + col * (boxSize + marginX);
          int y = startY + i * (boxSize + marginY);

          char pondKey[10];
          snprintf(pondKey, sizeof(pondKey), "%s%d", section.c_str(), boxNum);

          uint8_t status = 0;
          auto it = PondConfig->m_pondStatusMap.find(pondKey);
          if (it != PondConfig->m_pondStatusMap.end()) {
            status = it->second.Backup;
          }

          uint16_t fillColor = pondStatusColors[status];
          tft.fillRoundRect(x, y, boxSize, boxSize, 3, fillColor);
          tft.drawRoundRect(x, y, boxSize, boxSize, 3, fgColor());

          int textX = (boxNum < 10) ? (x + 5) : (x + 2);
          tft.setCursor(textX, y + 10);
          tft.setTextColor(fgColor());
          tft.print(boxNum);
        }
      }
      currentColX += colsNeeded * (boxSize + marginX); // shift by all columns drawn
    }
  }
}

// // -------- Right panel (pond matrix) --------
// void CDisplay::drawRightPanel(CPondConfig *PondConfig) {
//   if (!FirstTimeInRightPanel) {
//     if (PondConfig->m_pondStatusMap == lastPondStatusMap) return;
//   }
//   FirstTimeInRightPanel = false;

//   tft.fillRect(RIGHT_X(), CONTENT_Y(), RIGHT_W, CONTENT_H(), bgColor());
//   lastPondStatusMap = PondConfig->m_pondStatusMap;

//   std::map<char, std::vector<int>> pondGroups;
//   for (const auto& pair : PondConfig->m_pondStatusMap) {
//     const std::string& key = pair.first;
//     if (key.length() < 2) continue;
//     char section = key[0];
//     int number = atoi(key.substr(1).c_str());
//     pondGroups[section].push_back(number);
//   }

//   int boxSize = 16;
//   int marginX = 4;
//   int marginY = 3;
//   int startX  = RIGHT_X() + 2;
//   int startY  = CONTENT_Y() + 20;
//   int currentColX = startX;
//   int maxRows = 10;

//   tft.setFreeFont(&calibri_regular5pt7b);

//   bool firstSection = true;
//   for (const auto& group : pondGroups) {
//     char section = group.first;
//     std::vector<int> sortedNums = group.second;
//     std::sort(sortedNums.begin(), sortedNums.end());

//     int pondCount = (int)sortedNums.size();
//     int neededCols = (pondCount > 10) ? 2 : 1;

//     tft.setFreeFont(&calibri_regular10pt7b);
//     tft.setCursor(currentColX + 2, startY - 6);
//     tft.print(section);

//     tft.drawLine(SEP_X() + 1, startY - 2, SCREEN_WIDTH, startY - 2, fgColor());

//     if (!firstSection) {
//       tft.drawLine(currentColX - (marginX / 2), startY - 21,
//                    currentColX - (marginX / 2), startY + maxRows * (boxSize + marginY),
//                    fgColor());
//     }
//     firstSection = false;

//     tft.setFreeFont(&calibri_regular6pt7b);

//     if (neededCols == 1) {
//       for (int i = 0; i < pondCount && i < maxRows; i++) {
//         int x = currentColX;
//         int y = startY + i * (boxSize + marginY);
//         int boxNum = sortedNums[i];

//         char pondKey[6];
//         snprintf(pondKey, sizeof(pondKey), "%c%d", section, boxNum);

//         uint8_t status = 0;
//         auto it = PondConfig->m_pondStatusMap.find(pondKey);
//         if (it != PondConfig->m_pondStatusMap.end()) status = it->second.Backup;

//         uint16_t fillColor = pondStatusColors[status];
//         tft.fillRoundRect(x, y, boxSize, boxSize, 3, fillColor);
//         tft.drawRoundRect(x, y, boxSize, boxSize, 3, fgColor());

//         int textX = (boxNum < 10) ? (x + 5) : (x + 2);
//         tft.setCursor(textX, y + 10);
//         tft.setTextColor(fgColor());
//         tft.print(boxNum);
//       }
//       currentColX += (boxSize + marginX);
//     } else {
//       std::vector<int> leftBoxes, rightBoxes;
//       int half = (pondCount + 1) / 2;
//       for (int i = 0; i < half; i++) leftBoxes.push_back(sortedNums[i]);
//       for (int i = pondCount - 1; i >= half; i--) rightBoxes.push_back(sortedNums[i]);

//       for (int i = 0; i < maxRows; i++) {
//         int y = startY + i * (boxSize + marginY);

//         if (i < (int)leftBoxes.size()) {
//           int boxNum = leftBoxes[i];
//           int x = currentColX;
//           char pondKey[6];
//           snprintf(pondKey, sizeof(pondKey), "%c%d", section, boxNum);

//           uint8_t status = 0;
//           auto it = PondConfig->m_pondStatusMap.find(pondKey);
//           if (it != PondConfig->m_pondStatusMap.end()) status = it->second.Backup;

//           uint16_t fillColor = pondStatusColors[status];
//           tft.fillRoundRect(x, y, boxSize, boxSize, 3, fillColor);
//           tft.drawRoundRect(x, y, boxSize, boxSize, 3, fgColor());

//           int textX = (boxNum < 10) ? (x + 5) : (x + 2);
//           tft.setCursor(textX, y + 10);
//           tft.setTextColor(fgColor());
//           tft.print(boxNum);
//         }

//         if (i < (int)rightBoxes.size()) {
//           int boxNum = rightBoxes[i];
//           int x = currentColX + (boxSize + marginX);
//           char pondKey[6];
//           snprintf(pondKey, sizeof(pondKey), "%c%d", section, boxNum);

//           uint8_t status = 0;
//           auto it = PondConfig->m_pondStatusMap.find(pondKey);
//           if (it != PondConfig->m_pondStatusMap.end()) status = it->second.Backup;

//           uint16_t fillColor = pondStatusColors[status];
//           tft.fillRoundRect(x, y, boxSize, boxSize, 3, fillColor);
//           tft.drawRoundRect(x, y, boxSize, boxSize, 3, fgColor());

//           int textX = (boxNum < 10) ? (x + 5) : (x + 2);
//           tft.setCursor(textX, y + 10);
//           tft.setTextColor(fgColor());
//           tft.print(boxNum);
//         }
//       }
//       currentColX += 2 * (boxSize + marginX);
//     }
//   }
// }

// -------- Loading --------
void CDisplay::LoadingPage(void){
  drawCompanyLogo();
}

/*---------------Smart Config related Screens-------------*/


// void CDisplay::DisplaySmartConfig(String Myname, String MyPassKey)
// {
//   static int val = 0;
//   if (val == 0) ClearDisplay();
//   val = 1;

//   // Title
//   tft.setTextColor(fgColor());
//   tft.setFreeFont(&POPPINS_SEMIBOLD_016pt7b);
//   tft.setCursor(20, 30);
//   tft.print("Smart Config Setup");

//   tft.drawLine(0, 42, SCREEN_WIDTH, 42, fgColor());

//   // Step 1: Connect to Hotspot
//   tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
//   tft.setCursor(6, 65);
//   tft.print("1. Connect to this Wi-Fi:");

//   tft.setFreeFont(&calibri_regular10pt7b);
//   tft.setCursor(20, 85);
//   tft.print("SSID: ");
//   tft.print(Myname);

//   tft.setCursor(20, 105);
//   tft.print("Password: ");
//   tft.print(MyPassKey);

//   // QR for Wi-Fi
//   tft.setCursor(20, 125);
//   tft.setFreeFont(&calibri_regular10pt7b);
//   tft.print("Or just scan this QR:");
//   drawWifiQRCode(Myname, MyPassKey); // <-- function we discussed earlier

//   // Step 2: Open Setup Page
//   tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
//   tft.setCursor(6, 225);
//   tft.print("2. Open Setup Page:");

//   tft.setFreeFont(&calibri_regular10pt7b);
//   tft.setCursor(20, 245);
//   tft.print("URL: http://192.168.4.1");

//   // QR for Setup URL
//   tft.setCursor(20, 265);
//   tft.print("Scan to open directly:");
//   drawUrlQRCode("http://192.168.4.1"); // <-- similar QR function for URL

//   // Step 3: Enter credentials
//   tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
//   tft.setCursor(6, 305);
//   tft.print("3. Enter Wi-Fi & Save.");
// }

void CDisplay::printFOTA(int progress)
{
    tft.setTextColor(fgColor());
    tft.setFreeFont(&POPPINS_SEMIBOLD_012pt7b);
    tft.setCursor(10, 40);
    tft.print("Keep Device ON");
    tft.setCursor(20, 80);
    tft.print("Device");
    tft.setCursor(20, 130);
    tft.print("Firmware");
    tft.setCursor(20, 180);
    tft.print("Updating");
    tft.setCursor(20, 250);
    tft.setFreeFont(&POPPINS_SEMIBOLD_012pt7b);
    tft.print("Progress:");
    tft.fillRect(135, 230, 55, 40, bgColor());
    tft.setCursor(135, 250);
    tft.print(progress);
    tft.print("%");
}

void CDisplay::drawCompanyLogo() {
  tft.startWrite();
  for (int y = 0; y < 320; y++) {
    tft.setAddrWindow(0, y, 240, 1);
    for (int x = 0; x < 240; x++) {
      uint16_t color = pgm_read_word(&companyLogo[y * 240 + x]);
      tft.pushColor(color);
    }
  }
  tft.endWrite();
}

void CDisplay::DisplaySmartConfig(String Myname, String MyPassKey)
{
  static int val = 0;
  if (val == 0) ClearDisplay();
  val = 1;

  tft.setTextColor(fgColor());
  tft.setFreeFont(&POPPINS_SEMIBOLD_016pt7b);
  tft.setCursor(12, 32);
  tft.print("Smart Config");

  tft.drawLine(0, 42, SCREEN_WIDTH, 42, fgColor());
  tft.drawLine(0, 43, SCREEN_WIDTH, 43, fgColor());

  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  tft.setCursor(6, 65);  tft.print("1.Open Wifi in mobile");
  tft.setCursor(6, 112);  tft.print("2.Connect to hotspot");
  tft.setFreeFont(&calibri_regular10pt7b);
    tft.setCursor(20, 87); tft.print("Scan for Wifi networks");
  tft.setCursor(20, 137); tft.print("SSID: "); tft.print(Myname);
  tft.setCursor(20, 157); tft.print("Password: "); tft.print(MyPassKey);

  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  tft.setCursor(6, 183);  tft.print("3.Open the setup page");
  tft.setFreeFont(&calibri_regular10pt7b);
  tft.setCursor(20, 209); tft.print("URL: http://192.168.4.1");

  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  tft.setCursor(6, 243);  tft.print("4.Enter Wi-Fi credentials");
  tft.setFreeFont(&calibri_regular10pt7b);
  tft.setCursor(20, 269); tft.print("Then tap \"Save & Reboot\"");
}

/****************************
______________________
|    Saved!          |
|I will  Remember    |
|SSID: SAI           |
|PASS: Infi@2016     | 
|Restarting in 5s    |
----------------------
***********************************/
void CDisplay::DisplaySaveSmartConfig(String NewSsid, String NewPassword)
{
  static int val = 0;
  static uint32_t lastMillis = 0;
  if(val == 0) ClearDisplay();
  tft.setTextColor(fgColor(), bgColor());
  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  tft.setCursor(2, 25);
  tft.print("Applying Wi-Fi Settings");

  tft.drawLine(0, 48, SCREEN_WIDTH, 48, fgColor());
  tft.drawLine(0, 49, SCREEN_WIDTH, 49, fgColor());

  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  tft.setCursor(6, 82);   tft.print("SSID:");
  tft.setFreeFont(&calibri_regular10pt7b);
  tft.setCursor(70, 82);  tft.print(NewSsid);

  tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
  tft.setCursor(6, 114);  tft.print("Password:");
  tft.setFreeFont(&calibri_regular10pt7b);
  tft.setCursor(100, 114); tft.print(NewPassword);

  tft.setFreeFont(&calibri_regular10pt7b);
  tft.setCursor(2, 160);  tft.print("Saving credentials...");
  tft.setCursor(2, 190);  tft.print("Reconnecting to the network");

      // Only update countdown every 1 second
    static uint8_t SmartConfigcountdown = 5;
    if (millis() - lastMillis >= 1000 && SmartConfigcountdown >= 0) {
        lastMillis = millis();

        // Clear old countdown line only
        tft.fillRect(115, 210, 80, 30, bgColor());  // Adjust vertical position as needed

        tft.setFreeFont(&POPPINS_SEMIBOLD_09pt7b);
        tft.setTextColor(fgColor());
        tft.setCursor(5, 230);
        tft.print("Restarting in " + String(SmartConfigcountdown) + "s");
        SmartConfigcountdown--;
    }
}
