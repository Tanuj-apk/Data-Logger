#ifndef EEPROM_I2C_H_
#define EEPROM_I2C_H_

#include <stdint.h>
#include <stdbool.h>

/* I2C EEPROM Device Address (7-bit) */
#define EEPROM_ADDR    0x50

/* Initialization */
void I2C1_Init(void);

/* Basic EEPROM Functions */
void EEPROM_WriteByte_i2c(uint16_t memAddr, uint8_t data);
uint8_t EEPROM_ReadByte_i2c(uint16_t memAddr);

void EEPROM_WritePage_i2c(uint16_t memAddr,
                      volatile uint8_t *data,
                      uint8_t len);

void EEPROM_ReadPage_i2c(uint16_t pageStartAddr,
                     volatile uint8_t *buffer,
                     uint8_t len);

/* Pointer Storage Functions */
void EEPROM_WritePointer(uint32_t ptr);
uint32_t EEPROM_ReadPointer(void);

/* Test Functions */
void EEPROM_Write_Data(void);
void EEPROM_Read_Data(void);

#endif /* EEPROM_I2C_H_ */
