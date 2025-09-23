
#ifndef CAT24C32_H
#define CAT24C32_H

#include <stdint.h>

#define EE_ERROR_INVALID_LENGTH 2
#define EE_ERROR_INVALID_ADDR 1
#define EE_SUCCESS 0

#define EEPROM_SIM_SIZE  4096u //!< CAT24C32 EEPROM size.
#define EEPROM_SIM_SEQ_WRITE_MAX_BYTES    32
/* Maximum number of bytes writable to this slave emulator in one sequential access.
   Maximum allowed is 255.
 */
//<! Number of data bytes transfer in single request
#define EEPROM_SIM_SEQ_READ_MAX_BYTES     32
/* Slave memory addressing byte length */
#define EEPROM_SIM_ADDRESS_LEN_BYTES    2

#define CAT24C32_I2C_ADDRESS 0x57

int EE_IO_Read(uint16_t ssaddress, uint16_t address, uint8_t *p_data, uint8_t length);
int EE_IO_Write(uint16_t ssaddress, uint16_t address, uint8_t *p_data, uint8_t length);

class cCAT24C32
	{
	private:
                uint16_t EE_ByteErase(uint16_t);
                uint16_t EE_Write(uint16_t, uint8_t*, uint8_t);
                uint16_t EE_Read(uint16_t, uint8_t*, uint16_t);
	public:
                uint16_t SeqErase(uint16_t add, uint16_t length);
                void WriteInt(uint16_t Addr, uint16_t Data, uint8_t Size);
                void WriteFloat(uint16_t Addr,float Data,float Mf,uint8_t Size);
                uint16_t ReadInt(uint16_t Addr,uint8_t Size,uint16_t DefltVal,uint16_t IsValid);
                float Readfloat(uint16_t Addr,uint8_t Size,float Df,float DefltVal,float IsValid);
                void WriteArray(uint16_t Addr, uint8_t* Data, uint8_t Size);
                
#ifndef ISEEPROM
                uint16_t mDashReadInt(char * Var,char * dfltVal);
                float mDashReadFloat(char * Var,char * dfltVal);
                void mDashWriteInt(char * Var,uint16_t Val);
                void mDashWriteFloat(char * Var,float Val);
#endif
	};


#endif