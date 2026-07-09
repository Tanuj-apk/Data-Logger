#include <stdint.h>
#include <stdbool.h>

#include "inc/hw_memmap.h"
#include "inc/hw_i2c.h"
#include "inc/hw_types.h"

#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/pin_map.h"
#include "CH376S_uart.h"

#define EEPROM_ADDR 0x50
extern uint32_t g_sysClock;

///////////////////////////////////////////////////
////////////////// EEPROM /////////////////////////
///////////////////////////////////////////////////
bool WaitComplete(uint32_t base)
{
    uint32_t timeout = 1000000;
    while(timeout--)
    {
        if(HWREG(base + I2C_O_MRIS) & I2C_MRIS_RIS)
        {
            HWREG(base + I2C_O_MICR) = I2C_MICR_IC;
            return true;
        }
    }
    return false;
}

void EEPROM_WaitReady(void)
{
    while(1)
    {
        while(I2CMasterBusBusy(I2C1_BASE));
        I2CMasterSlaveAddrSet(I2C1_BASE, EEPROM_ADDR, false);
        I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_QUICK_COMMAND);
        if(WaitComplete(I2C1_BASE))
        {
            if(I2CMasterErr(I2C1_BASE) == I2C_MASTER_ERR_NONE)
            {
                return;
            }
        }
    }
}

void I2C1_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C1));

    GPIOPinConfigure(GPIO_PG0_I2C1SCL);
    GPIOPinConfigure(GPIO_PG1_I2C1SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTG_BASE, GPIO_PIN_0);
    GPIOPinTypeI2C(GPIO_PORTG_BASE, GPIO_PIN_1);
    I2CMasterInitExpClk(I2C1_BASE, g_sysClock, false);   // 100 kHz
}

void EEPROM_WriteByte_i2c(uint16_t memAddr, uint8_t data)
{
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    I2CMasterSlaveAddrSet(I2C1_BASE, EEPROM_ADDR, false);

    // Send High Address
    I2CMasterDataPut(I2C1_BASE, (memAddr >> 8) & 0xFF);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_START);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    // Send Low Address
    I2CMasterDataPut(I2C1_BASE, memAddr & 0xFF);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    // Send Data
    I2CMasterDataPut(I2C1_BASE, data);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);
}

uint8_t EEPROM_ReadByte_i2c(uint16_t memAddr)
{
    uint8_t data;
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);
    I2CMasterSlaveAddrSet(I2C1_BASE, EEPROM_ADDR, false);

    // Send High Address
    I2CMasterDataPut(I2C1_BASE, (memAddr >> 8) & 0xFF);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_START);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    // Send Low Address
    I2CMasterDataPut(I2C1_BASE, memAddr & 0xFF);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    // Switch to Read Mode
    I2CMasterSlaveAddrSet(I2C1_BASE, EEPROM_ADDR, true);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    data = I2CMasterDataGet(I2C1_BASE);
    return data;
}

void EEPROM_WritePage_i2c(uint16_t memAddr, volatile uint8_t *data, uint8_t len)
{
    uint8_t i;
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    I2CMasterSlaveAddrSet(I2C1_BASE, EEPROM_ADDR, false);
    I2CMasterDataPut(I2C1_BASE, (memAddr >> 8) & 0xFF);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_START);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    I2CMasterDataPut(I2C1_BASE, memAddr & 0xFF);
    I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
//    while(I2CMasterBusy(I2C1_BASE));
    WaitComplete(I2C1_BASE);

    for(i = 0; i < len; i++)
    {
        I2CMasterDataPut(I2C1_BASE, data[i]);

        if(i == (len - 1))
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
        else
            I2CMasterControl(I2C1_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);

//        while(I2CMasterBusy(I2C1_BASE));
        WaitComplete(I2C1_BASE);
    }
}
void EEPROM_ReadPage_i2c(uint16_t pageStartAddr, volatile uint8_t *buffer, uint8_t len)
{
    uint8_t i;

    for(i = 0; i < len; i++)
    {
        buffer[i] = EEPROM_ReadByte_i2c(pageStartAddr + i);
    }
}

void EEPROM_Write_Data()
{
// -------- Byte Write --------
    EEPROM_WriteByte_i2c(0x0000, 0x54); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0001, 0x41); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0002, 0x4E); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0003, 0x55); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0004, 0x4A); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0005, 0x20); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0006, 0x53); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0007, 0x4F); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0008, 0x4E); delay_ms(6);
    EEPROM_WriteByte_i2c(0x0009, 0x49); delay_ms(6);
    EEPROM_WriteByte_i2c(0x000A, 0x00); delay_ms(6);
    delay_ms(100);
}
void EEPROM_Read_Data()
{
    // -------- Byte Read --------
    EEPROM_ReadByte_i2c(0x0000); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0001); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0002); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0003); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0004); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0005); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0006); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0007); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0008); delay_ms(6);
    EEPROM_ReadByte_i2c(0x0009); delay_ms(6);
    EEPROM_ReadByte_i2c(0x000A); delay_ms(6);
}

void EEPROM_WritePointer(uint32_t ptr)
{
    EEPROM_WaitReady();
    EEPROM_WritePage_i2c(0x0000, (uint8_t *)&ptr, 4);
//    delay_ms(10);      // wait for EEPROM internal write cycle
}

uint32_t EEPROM_ReadPointer(void)
{
    EEPROM_WaitReady();
    uint32_t ptr;
    EEPROM_ReadPage_i2c(0x0000, (uint8_t *)&ptr, 4);
    return ptr;
}
