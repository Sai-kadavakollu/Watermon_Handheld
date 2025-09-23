#ifndef BSP_H
#define BSP_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <Wire.h>
#include "time.h"
#include "PCF85063A.h"
#include "Arduino.h"
#include <SPI.h>

#define RELAYON     1
#define RELAYOFF    0

// #define TFT_MISO -1
// #define TFT_SCK 18
// #define TFT_MOSI 23
// #define TFT_DC 19
// #define TFT_RESET 5
// #define TFT_CS -1
// #define TFT_BACKLIGHT 25

/*LED GPIO PINS DEFINITIONS*/
#define BSP_LED_1    2

/*Button State for detection*/
#define BUTTON_IDLE 0xff
#define BUTTONUP 0


/*BUTTON GPIO PINS DEFINITIONS*/
#define BSP_BTN_1              32  //Up

#define ON  0x01
#define OFF 0X00

/*I2C GPIO PINS DEFINITIONS are being used for display*/
#define BSP_SDA             21
#define BSP_SCL             22

/*Buzzer / Hooter Pin*/
#define BUZZER_PIN     4  
#define WDT_TIMEOUT    60


/*PCB EXTERNAL GPIO DEFINITIONS */
#define BSP_PWR_DET       35

/*VBat Monitoring section*/
#define VBAT_ADC  34

class cBsp
{
    public:
        bool m_bIsRtcSynced;
        float pcbMake;     
    
        /*Construct*/
        cBsp(void);
        /*Destruct*/
        ~cBsp(void);
        /*Functions*/
        void setPcbMake(float);       
        void OnOffRelay(uint8_t relayNo,uint8_t state);
        void gpioInitialization(void);
        void BSP_IOwtire(int Pin,int st);
        void i2cInitialization(void);
        // void spiInitialization(void);
        void indLedToggle(void);
        void indLedOff(void);
        uint8_t getButtonEvent(void);
        int syncRTCTime(cPCF85063A*, time_t, time_t);
        time_t ConvertEpoch(cPCF85063A* LoclRTC);
        void ioPinWrite(uint8_t , bool );
        uint8_t ioPinRead(uint8_t pin);
        void wdtInit(void);
        void wdtAdd(TaskHandle_t );
        void wdtfeed(void);
        void hooterInit(void);
        void hooterOn(void);
        void hooterOff(void);
};

#endif
