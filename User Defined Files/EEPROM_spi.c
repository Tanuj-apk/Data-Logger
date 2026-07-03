#include "EEPROM_spi.h"
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/ssi.h"
#include "driverlib/pin_map.h"

extern uint32_t g_sysClock;

void EEPROM_SPI_Init(void)
{
    GPIOPinConfigure(GPIO_PB5_SSI1CLK);
    //GPIOPinConfigure(GPIO_PB4_GPIO);
    GPIOPinConfigure(GPIO_PE4_SSI1XDAT0);
    GPIOPinConfigure(GPIO_PE5_SSI1XDAT1);

    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_5);
    GPIOPinTypeSSI(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    GPIOPinTypeGPIOOutput(EEPROM_CS_PORT, EEPROM_CS_PIN);
//    GPIOPadConfigSet(EEPROM_CS_PORT, EEPROM_CS_PIN,
//                     GPIO_STRENGTH_12MA,
//                     GPIO_PIN_TYPE_STD);
    EEPROM_CS_HIGH();

    SSIConfigSetExpClk(SSI1_BASE,
                       g_sysClock,
                       SSI_FRF_MOTO_MODE_3,
                       SSI_MODE_MASTER,
                       1000000,
                       8);

    SSIEnable(SSI1_BASE);

    uint32_t dummy;
    while(SSIDataGetNonBlocking(SSI1_BASE,&dummy));
//
//    /* Clear protection bits */
//    EEPROM_WriteEnable();
//
//    EEPROM_CS_LOW();
//    SPI_TxRx(CMD_CE);
//    EEPROM_CS_HIGH();
//
//    EEPROM_WaitBusy();
}

uint8_t SPI_TxRx(uint8_t data)
{
    uint32_t r;
    SSIDataPut(SSI1_BASE, data);
    while(SSIBusy(SSI1_BASE));
    SSIDataGet(SSI1_BASE, &r);
    return (uint8_t)r;
}

void EEPROM_WriteEnable(void)
{
    EEPROM_CS_LOW();
    SPI_TxRx(CMD_WREN);
    EEPROM_CS_HIGH();
}

void EEPROM_WaitBusy(void)
{
    uint8_t status;

    do{
        EEPROM_CS_LOW();
        SPI_TxRx(CMD_RDSR);
        status = SPI_TxRx(0xFF);
        EEPROM_CS_HIGH();
    }
    while(status & 0x01);
}

void Flash_SectorErase(uint32_t addr)
{
    EEPROM_WriteEnable();

    EEPROM_CS_LOW();

    SPI_TxRx(CMD_SE);

    SPI_TxRx(addr>>16);
    SPI_TxRx(addr>>8);
    SPI_TxRx(addr);

    EEPROM_CS_HIGH();

    EEPROM_WaitBusy();
}

void EEPROM_PageProgram(uint32_t addr, volatile uint8_t *data, uint16_t len)
{
    uint16_t i;

    EEPROM_WriteEnable();

    EEPROM_CS_LOW();

    SPI_TxRx(CMD_PP);

    SPI_TxRx(addr>>16);
    SPI_TxRx(addr>>8);
    SPI_TxRx(addr);

    for(i=0;i<len;i++)
        SPI_TxRx(data[i]);

    EEPROM_CS_HIGH();

    EEPROM_WaitBusy();
}

uint8_t EEPROM_ReadByte(uint32_t addr)
{
    uint8_t d;

    EEPROM_CS_LOW();

    SPI_TxRx(CMD_READ);

    SPI_TxRx(addr>>16);
    SPI_TxRx(addr>>8);
    SPI_TxRx(addr);

    d = SPI_TxRx(0xFF);

    EEPROM_CS_HIGH();

    return d;
}

void EEPROM_ReadPage(uint32_t addr,volatile uint8_t *buf,volatile uint16_t len)
{
    uint16_t i;

    EEPROM_CS_LOW();

    SPI_TxRx(CMD_READ);

    SPI_TxRx(addr>>16);
    SPI_TxRx(addr>>8);
    SPI_TxRx(addr);

    for(i=0;i<len;i++)
        buf[i] = SPI_TxRx(0xFF);

    EEPROM_CS_HIGH();
}
