
#include "CAT24C32.h"
#include <Wire.h>
#include <stdint.h>
#include <string.h>
#include "Arduino.h"

/*************************************************************
 * EE_Read() function is used to read data from EEPROM
 * ARGUMENTS: 
 *          address : The memory address from where to read
 *          p_data  : Integer Pointer to store the data read from
 *                    EEPROM
 *          length  : No. of Bytes to fetch from the EEPROOM
 * RETURN: 
 *          1 : EEPROM read is Successfull
 *          0 : EEPROM read error 
 * ***********************************************************/
uint16_t cCAT24C32::EE_Read(uint16_t address, uint8_t *p_data, uint16_t length)
{    
    return EE_IO_Read(CAT24C32_I2C_ADDRESS,address,p_data,length);
}


/*************************************************************
 * EE_Write() function is used to write data to EEPROM
 * ARGUMENTS: 
 *          address: Address of the memory where we have to 
 *                   write data
 *          p_data : The memory address to write data into EEPROM
 *          length  : No. of Bytes to write to the EEPROOM
 * RETURN: 
 *          1 : EEPROM write is Successfull
 *          0 : EEPROM write error 
 * ***********************************************************/
uint16_t cCAT24C32::EE_Write(uint16_t address, uint8_t * p_data, uint8_t length)
{

    /* Memory device supports only a limited number of bytes written in sequence */
    if (length > (EEPROM_SIM_SEQ_WRITE_MAX_BYTES))
    {
      return EE_ERROR_INVALID_LENGTH;	
    }
		
    for(uint8_t i=0;i<length ;i++)
    {
        uint16_t temp_address = address + i ;
    
        /* All written data must be in the same page */
        if ((temp_address / (EEPROM_SIM_SEQ_WRITE_MAX_BYTES)) != ((temp_address + length - 1) / (EEPROM_SIM_SEQ_WRITE_MAX_BYTES)))
        {
                return EE_ERROR_INVALID_ADDR;
        }     
    }

    return EE_IO_Write(CAT24C32_I2C_ADDRESS,address,p_data,length);	
}


/*************************************************************
 * EE_ByteErase() function is used to erase data from particular 
 * address of EEPROM (Writting 0xFF)
 * ARGUMENTS: 
 *          address: Address of the memory where we have to 
 *                   erase data
 * RETURN: 
 *          1 : EEPROM erase is Successfull
 * ***********************************************************/
uint16_t cCAT24C32::EE_ByteErase(uint16_t address)
{            
    Wire.beginTransmission(CAT24C32_I2C_ADDRESS);
    Wire.write((int)(address >> 8));   // MSB
    Wire.write((int)(address & 0xFF)); // LSB
    Wire.write(0xff);
    Wire.endTransmission();
    delay(5);
    return 1;	
}

/*************************************************************
 * SeqErase() function is used to erase data to particular 
 * sequence address of EEPROM (Writting 0xFF)
 * ARGUMENTS: 
 *          add     : Address of the memory where we have to 
 *                    start erasing EEPROM data
 *          length  : length of sequence to erase 
 * RETURN: 
 *          1 : EEPROM erase is Successfull
 * ***********************************************************/
uint16_t  cCAT24C32::SeqErase(uint16_t add, uint16_t length)
{  
    for(uint8_t i=0;i<length;i++)
    {
        uint16_t address = add + i;
        EE_ByteErase(address);
    }
    return 1;	
}

/*************************************************************
 * WriteArray() function is used to write an array to EEPROM
 * ARGUMENTS: 
 *          Addr: Address of the memory where we have to 
 *                   write array data into EEPROM memory
 *          Data : The memory address to write data into EEPROM
 *          Size  : No. of Bytes to write to the EEPROOM
 * ***********************************************************/
void cCAT24C32::WriteArray(uint16_t Addr, uint8_t* Data, uint8_t Size)
{
  uint8_t Buff[3] = {0};
  for(uint8_t len = 0;len<Size;len++){
    Buff[0] = (uint8_t)(Data[len]);
    EE_Write((Addr+len),Buff,1);
  }
}

/*************************************************************
 * WriteInt() function is used to write an array to EEPROM
 * ARGUMENTS: 
 *          Addr: Address of the memory where we have to 
 *                   write array data into EEPROM memory
 *          Data : The memory address to write data into EEPROM
 *          Size  : No. of Bytes to write to the EEPROOM
 * ***********************************************************/
void cCAT24C32::WriteInt(uint16_t Addr, uint16_t Data, uint8_t Size)
{
  uint8_t Buff[3] = {0};
  if(Size == 2){ 
    Buff[0] = (uint8_t)(Data >> 8);
    Buff[1] = (uint8_t)(Data);
    EE_Write(Addr,Buff,2);
  }
  else if(Size == 1){
    Buff[0] = (uint8_t)(Data);
    EE_Write(Addr,Buff,1);
  }
}

/*************************************************************
 * WriteFloat() function is used to write an array to EEPROM
 * ARGUMENTS: 
 *          Addr: Address of the memory where we have to 
 *                   write array data into EEPROM memory
 *          Data : The memory address to write data into EEPROM
 *          Mf   : Multiplication Factor 
 *          Size : No. of Bytes to write to the EEPROOM
 * ***********************************************************/
void cCAT24C32::WriteFloat(uint16_t Addr,float Data,float Mf,uint8_t Size)
{
//   uint8_t Buff[3] = {0};
  uint16_t Val = (uint16_t)(Data * Mf);
  WriteInt(Addr,Val,Size);
}

/*************************************************************
 * ReadInt() function is used to write an array to EEPROM
 * ARGUMENTS: 
 *          Addr: Address of the memory where we have to 
 *                   write array data into EEPROM memory
 *          Data : The memory address to write data into EEPROM
 *          DefltVal   : Default value.
 *          IsValid : Maximum range of value that is valid.
 * RETURN: 
 *         RetVal   : Returns 16 bit read value. 
 * ***********************************************************/
uint16_t cCAT24C32::ReadInt(uint16_t Addr,uint8_t Size,uint16_t DefltVal,uint16_t IsValid)
{
  uint8_t RBuff[3] = {0};
  uint16_t RetVal = 0;
  EE_Read(Addr,RBuff,Size);
  delay(5);
  if(Size == 2){ 
    uint16_t Vld16 = (uint16_t)((RBuff[0] << 8) | (RBuff[1]));
    if(Vld16 > IsValid)
      RetVal = DefltVal;
    else
      RetVal = (uint16_t)(Vld16);
  }
  else if(Size == 1){
    uint8_t Vld8 = (uint8_t)(RBuff[0]);
    if(Vld8 > IsValid)
      RetVal = DefltVal;
    else
      RetVal = (uint8_t)(Vld8);
  }
  return RetVal;
}

/*************************************************************
 * ReadInt() function is used to write an array to EEPROM
 * ARGUMENTS: 
 *          Addr: Address of the memory where we have to 
 *                   write array data into EEPROM memory
 *          Size : Number of bytes to read from EEPROM
 *          Df   : Divide Factor
 *          DefltVal   : Default value.
 *          IsValid : Maximum range of value that is valid.
 * RETURN: 
 *         RetVal   : Returns 16 bit read value. 
 * ***********************************************************/
float cCAT24C32::Readfloat(uint16_t Addr,uint8_t Size,float Df,float DefltVal,float IsValid)
{
  uint8_t RBuff[3] = {0};
  float RetVal = 0;
  EE_Read(Addr,RBuff,Size);
  delay(5);
  if(Size == 2){ 
    float Vld = (float)(((RBuff[0] << 8) | (RBuff[1]))/Df);
    if(Vld > IsValid)
      RetVal = (float)(DefltVal);
    else
      RetVal = (float)(Vld);
  }
  else if(Size == 1){
    float Vld = (float)(RBuff[0]/Df);
    if(Vld > IsValid)
      RetVal = (float)(DefltVal);
    else
      RetVal = (float)(Vld);
  }
  return RetVal;
}
