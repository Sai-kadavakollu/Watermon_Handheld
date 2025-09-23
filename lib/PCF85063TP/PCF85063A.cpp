/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Jaakko Salo (jaakkos@gmail.com / jaakkos on Freenode)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include "PCF85063A.h"
#include "Arduino.h"


#define REG_CTRL1_ADDR                   0x00
#define REG_CTRL2_ADDR                   0x01
#define REG_TIME_DATE_ADDR               0x04
#define REG_ALARM_ADDR                   0x0B
#define REG_OFFSET_ADDR                  0x02
#define REG_COUNTDOWN_TIMER_VALUE_ADDR   0x10
#define REG_COUNTDOWN_TIMER_MODE_ADDR    0x11

#define I2C_ADDR                         0x51

/********************************************************************
 *@brief  Function To convert bcd value to integer
 *
 *@param[in]   bcd    : BCD value.
 *@param[out]  return : Unsigned integer value
 ********************************************************************/
uint8_t cPCF85063A::bcd_decode(uint8_t bcd)
{
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/********************************************************************
 *@brief  Function To convert integer value to BCD
 *
 *@param[in]   dec    : Decimal value.
 *@param[out]  return : BCD value
 ********************************************************************/
uint8_t cPCF85063A::bcd_encode(uint8_t dec)
{
  return ((dec / 10) << 4) | (dec % 10);
}

cPCF85063A::cPCF85063A()
{
  // Wire.begin();
}

/********************************************************************
 *@brief  Function To convert bcd value to integer
 *
 *@param[in]   bcd    : BCD value.
 *@param[out]  return : Unsigned integer value
 ********************************************************************/
bool cPCF85063A::getTime()
{
  uint8_t buf[7];

  if (!RTC_IO_Read(I2C_ADDR, REG_TIME_DATE_ADDR, sizeof(buf), buf))
  {
    return false; 
  }

  this->m_iSecond      = bcd_decode(buf[0] & ~0x80);
  this->m_iMinute      = bcd_decode(buf[1] & ~0x80);
  this->m_iHour        = bcd_decode(buf[2] & ~0xC0); /* 24h clock */
  this->m_iDayOfMonth  = bcd_decode(buf[3] & ~0xC0);
  this->m_iDayOfWeek   = bcd_decode(buf[4] & ~0xF8);
  this->m_iMonth       = bcd_decode(buf[5] & ~0xE0);
  this->m_iYear        = bcd_decode(buf[6]);
  // Serial.print("RTC : ");
  // Serial.println(buf[0],HEX);
  //return !(buf[0] & 0x80);
  return true;
}

/********************************************************************
 *@brief  Function To set the time on RTC module
 *
 *@param[in]   bcd    : BCD value.
 *@param[out]  returm : true if success or false if failed to update 
 *                      RTC module
 ********************************************************************/
bool cPCF85063A::setTime()
{
  uint8_t buf[7];

  buf[0] = bcd_encode(this->m_iSecond );
  buf[1] = bcd_encode(this->m_iMinute);
  buf[2] = bcd_encode(this->m_iHour);
  buf[3] = bcd_encode(this->m_iDayOfMonth);
  buf[4] = bcd_encode(this->m_iDayOfWeek);
  buf[5] = bcd_encode(this->m_iMonth);
  buf[6] = bcd_encode(this->m_iYear);

  return RTC_IO_Write(I2C_ADDR, REG_TIME_DATE_ADDR, sizeof(buf), buf);
}

/********************************************************************
 *@brief  Function To set public variables hour , minutes and seconds
 *        of PCF85063A class 
 *
 *@param[in]   _hour  : Current Hour 
 *@param[in]  _minute : Current Minute
 *@param[in]  _second : Current Second
 ********************************************************************/
void cPCF85063A::fillByHMS(uint8_t _hour, uint8_t _minute, uint8_t _second)
{
  // assign variables
  this->m_iHour = _hour;
  this->m_iMinute = _minute;
  this->m_iSecond = _second;
}

/********************************************************************
 *@brief  Function To set public variables hour , minutes and seconds
 *        of PCF85063A class 
 *
 *@param[in]   _year : Current Year 
 *@param[in]  _month : Current Month
 *@param[in]  _day   : Current day
 ********************************************************************/
void cPCF85063A::fillByYMD(uint16_t _year, uint8_t _month, uint8_t _day)
{
  this->m_iYear = _year-2000;
  this->m_iMonth = _month;
  this->m_iDayOfMonth = _day;
}

/********************************************************************
 *@brief  Function To set public variables hour , minutes and seconds
 *        of PCF85063A class 
 *
 *@param[in]   _dow : Current Day of the Weak 
 ********************************************************************/
void cPCF85063A::fillDayOfWeek(uint8_t _dow)
{
  this->m_iDayOfWeek = _dow;
}

// bool cPCF85063A::reset()
// {
//   uint8_t buf = 0x58;
//   return RTC_IO_Write(I2C_ADDR, REG_CTRL1_ADDR, 1, &buf);
// }

// bool PCF85063A::stop(bool stopped)
// {
//   PCF85063A_Regs regs;

//   if (!this->ctrl_get(&regs))
//     return false;

//   if (stopped) PCF85063A_REG_SET(regs, PCF85063A_REG_STOP);
//   else         PCF85063A_REG_CLEAR(regs, PCF85063A_REG_STOP);

//   return this->ctrl_set(regs, true);
// }

// bool PCF85063A::clkout_freq_set(uint16_t freq)
// {
//   uint8_t COF;
//   PCF85063A_Regs regs;

//   switch (freq)
//   {
//     case 0:     COF = 7; break;
//     case 1:     COF = 6; break;
//     case 1024:  COF = 5; break;
//     case 2048:  COF = 4; break;
//     case 4096:  COF = 3; break;
//     case 8192:  COF = 2; break;
//     case 16384: COF = 1; break;
//     case 32768: COF = 0; break;
//     default: return false;
//   }

//   if (!this->ctrl_get(&regs))
//     return false;

//   PCF85063A_REG_CLEAR(regs, PCF85063A_REG_COF);
//   regs |= ((uint16_t)COF) << 8;

//   return this->ctrl_set(regs, true);
// }

// bool PCF85063A::countdown_set(bool enable,
//                          CountdownSrcClock source_clock,
//                          uint8_t value,
//                          bool int_enable,
//                          bool int_pulse)
// {
//   uint8_t timer_reg[2] = {0};

//   if (source_clock < 0 || source_clock > 3)
//     return false;

//   /* First disable the countdown timer */
//   if (!RTC_IO_Write(I2C_ADDR, REG_COUNTDOWN_TIMER_MODE_ADDR, 1, timer_reg+1))
//     return false;

//   /* Reconfigure timer */
//   if (enable) timer_reg[1] |= PCF85063A_REG_TE;
//   if (int_enable) timer_reg[1] |= PCF85063A_REG_TIE;
//   if (int_pulse) timer_reg[1] |= PCF85063A_REG_TI_TP;
//   timer_reg[1] |= source_clock << 3;
//   timer_reg[0] = value;

//   return RTC_IO_Write(I2C_ADDR, REG_COUNTDOWN_TIMER_VALUE_ADDR, 2, timer_reg);
// }

// bool PCF85063A::countdown_get(uint8_t *value)
// {
//   return RTC_IO_Read(I2C_ADDR, REG_COUNTDOWN_TIMER_VALUE_ADDR, 1, value);
// }

// bool PCF85063A::alarm_set(int second, int minute, int hour, int day,
//                      int weekday)
// {
//   uint8_t buf[5];

//   if ((second < 0 || second > 59) && second != -1) return false;
//   if ((minute < 0 || minute > 59) && minute != -1) return false;
//   if ((hour < 0 || hour > 23) && hour != -1) return false;
//   if ((day < 0 || day > 31) && day != -1) return false;
//   if ((weekday < 0 || weekday > 6) && weekday != -1) return false;

//   buf[0] = second < 0 ? 0x80 : bcd_encode(second);
//   buf[1] = minute < 0 ? 0x80 : bcd_encode(minute);
//   buf[2] = hour < 0 ? 0x80 : bcd_encode(hour);
//   buf[3] = day < 0 ? 0x80 : bcd_encode(day);
//   buf[4] = weekday < 0 ? 0x80 : bcd_encode(weekday);

//   return RTC_IO_Write(I2C_ADDR, REG_ALARM_ADDR, sizeof(buf), buf);
// }

// bool PCF85063A::ctrl_get(PCF85063A_Regs *regs)
// {
//   uint8_t buf[2];

//   if (!RTC_IO_Read(I2C_ADDR, REG_CTRL1_ADDR, sizeof(buf), buf))
//     return false;

//   *regs = buf[0];
//   *regs |= ((uint16_t)buf[1]) << 8;

//   return true;
// }

// bool PCF85063A::ctrl_set(PCF85063A_Regs regs, bool mask_alarms)
// {
//   uint8_t buf[2];
//   // int wrsz;

//   if (mask_alarms)
//     regs &= ~(PCF85063A_REG_AF | PCF85063A_REG_TF);

//   buf[0] = regs & 0xFF;
//   buf[1] = regs >> 8;

//   return RTC_IO_Write(I2C_ADDR, REG_CTRL1_ADDR, sizeof(buf), buf);
// }
