#ifndef EEPROM_SPI_H_
#define EEPROM_SPI_H_

#include <stdint.h>

/* Commands */
//#define EEPROM_CMD_WREN   0x06
//#define EEPROM_CMD_RDSR   0x05
//#define EEPROM_CMD_READ   0x03
//#define EEPROM_CMD_PGER   0xDB
//#define EEPROM_CMD_PGPR   0x0A
//#define EEPROM_CMD_WRSR   0x01

#define CMD_WREN   0x06
#define CMD_RDSR   0x05
#define CMD_READ   0x03
#define CMD_PP     0x02      // Page Program
#define CMD_SE     0x20      // Sector Erase (4KB)
#define CMD_CE     0xC7      // Chip Erase

/* CS pin */
#define EEPROM_CS_PORT GPIO_PORTB_BASE
#define EEPROM_CS_PIN  GPIO_PIN_4

#define EEPROM_CS_LOW()  GPIOPinWrite(EEPROM_CS_PORT, EEPROM_CS_PIN, 0)
#define EEPROM_CS_HIGH() GPIOPinWrite(EEPROM_CS_PORT, EEPROM_CS_PIN, EEPROM_CS_PIN)

void EEPROM_SPI_Init(void);
uint8_t SPI_TxRx(uint8_t data);

void EEPROM_WriteEnable(void);
void EEPROM_WaitBusy(void);

void Flash_SectorErase(uint32_t addr);
void EEPROM_PageProgram(uint32_t addr, volatile uint8_t *data, uint16_t len);

uint8_t EEPROM_ReadByte(uint32_t addr);
void EEPROM_ReadPage(uint32_t addr, volatile uint8_t *buf, uint16_t len);

#endif
