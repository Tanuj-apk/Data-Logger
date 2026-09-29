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
    EEPROM_CS_HIGH();

    SSIConfigSetExpClk(SSI1_BASE, g_sysClock, SSI_FRF_MOTO_MODE_3, SSI_MODE_MASTER, 1000000, 8);
    SSIEnable(SSI1_BASE);

    uint32_t dummy;
    while(SSIDataGetNonBlocking(SSI1_BASE,&dummy));
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

//#include "EEPROM_spi.h"
//#include "inc/hw_memmap.h"
//#include "driverlib/gpio.h"
//#include "driverlib/ssi.h"
//#include "driverlib/pin_map.h"
//
//extern uint32_t g_sysClock;
//
///* Set whenever the SPI flash driver detects a communication failure. */
//static volatile bool g_flash_spi_fault = false;
//
///* ============================================================
// * INTERNAL HELPERS
// * ========================================================== */
//static bool SPI_PutByte(uint8_t data)
//{
//    uint32_t timeout = SPI_TRANSFER_TIMEOUT_LOOPS;
//
//    while(!SSIDataPutNonBlocking(SSI1_BASE, (uint32_t)data))
//    {
//        if(timeout == 0U)
//        {
//            return false;
//        }
//        timeout--;
//    }
//    return true;
//}
//
//static bool SPI_GetByte(uint8_t *data)
//{
//    uint32_t value;
//    uint32_t timeout = SPI_TRANSFER_TIMEOUT_LOOPS;
//
//    if(data == NULL)
//    {
//        return false;
//    }
//
//    while(!SSIDataGetNonBlocking(SSI1_BASE, &value))
//    {
//        if(timeout == 0U)
//        {
//            return false;
//        }
//        timeout--;
//    }
//
//    *data = (uint8_t)value;
//    return true;
//}
//
///* ============================================================
// * INITIALIZATION
// * ========================================================== */
//void EEPROM_SPI_Init(void)
//{
//    uint32_t dummy;
//    g_flash_spi_fault = false;
//
//    /* SSI1:
//     *   PB5 = SSI1CLK
//     *   PE4 = SSI1XDAT0
//     *   PE5 = SSI1XDAT1
//     * SPI mode 3, 1 MHz, 8 bits. */
//    GPIOPinConfigure(GPIO_PB5_SSI1CLK);
//    GPIOPinConfigure(GPIO_PE4_SSI1XDAT0);
//    GPIOPinConfigure(GPIO_PE5_SSI1XDAT1);
//
//    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_5);
//    GPIOPinTypeSSI(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);
//
//    /* Flash CS is manually controlled as GPIO. */
//    GPIOPinTypeGPIOOutput(EEPROM_CS_PORT, EEPROM_CS_PIN);
//    EEPROM_CS_HIGH();
//
//    SSIConfigSetExpClk(SSI1_BASE, g_sysClock, SSI_FRF_MOTO_MODE_3, SSI_MODE_MASTER, 1000000U, 8U);
//    SSIEnable(SSI1_BASE);
//
//    /* Empty any stale RX data left in the SSI FIFO. */
//    while(SSIDataGetNonBlocking(SSI1_BASE, &dummy))
//    {
//        /* Intentionally empty. */
//    }
//}
//
///* ============================================================
// * ONE SPI BYTE TRANSFER
// * ========================================================== */
//bool SPI_TxRx(uint8_t tx, uint8_t *rx)
//{
//    uint8_t received;
//
//    if(rx == NULL)
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    if(!SPI_PutByte(tx))
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    /* Wait for this transmitted byte to finish, with a bound. */
//    {
//        uint32_t timeout = SPI_TRANSFER_TIMEOUT_LOOPS;
//        while(SSIBusy(SSI1_BASE))
//        {
//            if(timeout == 0U)
//            {
//                g_flash_spi_fault = true;
//                return false;
//            }
//            timeout--;
//        }
//    }
//
//    if(!SPI_GetByte(&received))
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    *rx = received;
//    return true;
//}
//
///* ============================================================
// * READ STATUS REGISTER
// * ========================================================== */
//bool EEPROM_ReadStatus(uint8_t *status)
//{
//    uint8_t rx;
//
//    if(status == NULL)
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    EEPROM_CS_LOW();
//
//    if(!SPI_TxRx(CMD_RDSR, &rx) || !SPI_TxRx(0xFFU, &rx))
//    {
//        EEPROM_CS_HIGH();
//        return false;
//    }
//
//    *status = rx;
//    EEPROM_CS_HIGH();
//    return true;
//}
//
///* ============================================================
// * WRITE ENABLE
// * ========================================================== */
//bool EEPROM_WriteEnable(void)
//{
//    uint8_t rx;
//    uint8_t status;
//
//    EEPROM_CS_LOW();
//
//    if(!SPI_TxRx(CMD_WREN, &rx))
//    {
//        EEPROM_CS_HIGH();
//        return false;
//    }
//
//    EEPROM_CS_HIGH();
//
//    /* Verify WEL = bit 1 of the status register. */
//    if(!EEPROM_ReadStatus(&status))
//    {
//        return false;
//    }
//
//    if((status & 0x02U) == 0U)
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    return true;
//}
//
///* ============================================================
// * WAIT UNTIL FLASH IS READY
// * WIP = status bit 0.
// * A repeated 0xFF status is treated as a missing/non-responsive
// * flash device. This makes a floating MISO line fail quickly
// * instead of waiting through the entire timeout.
// * ========================================================== */
//bool Flash_WaitReady(uint32_t timeout_polls)
//{
//    uint8_t status;
//    uint32_t invalid_status_count = 0U;
//
//    while(timeout_polls > 0U)
//    {
//        if(!EEPROM_ReadStatus(&status))
//        {
//            return false;
//        }
//
//        if(status == 0xFFU)
//        {
//            invalid_status_count++;
//            if(invalid_status_count >= FLASH_INVALID_STATUS_LIMIT)
//            {
//                g_flash_spi_fault = true;
//                return false;
//            }
//        }
//        else
//        {
//            invalid_status_count = 0U;
//        }
//
//        if((status & 0x01U) == 0U)
//        {
//            return true;
//        }
//
//        timeout_polls--;
//    }
//
//    g_flash_spi_fault = true;
//    return false;
//}
//
///* ============================================================
// * SECTOR ERASE
// * ========================================================== */
//bool Flash_SectorErase(uint32_t addr)
//{
//    uint8_t rx;
//
//    if(!EEPROM_WriteEnable())
//    {
//        return false;
//    }
//
//    EEPROM_CS_LOW();
//
//    if(!SPI_TxRx(CMD_SE, &rx) || !SPI_TxRx((uint8_t)(addr >> 16U), &rx) ||
//       !SPI_TxRx((uint8_t)(addr >> 8U), &rx) || !SPI_TxRx((uint8_t)addr, &rx))
//    {
//        EEPROM_CS_HIGH();
//        return false;
//    }
//
//    EEPROM_CS_HIGH();
//    return Flash_WaitReady(FLASH_ERASE_TIMEOUT_POLLS);
//}
//
///* ============================================================
// * PAGE PROGRAM
// * The caller must keep len within one 256-byte flash page.
// * Flash_AppendEventRecord() already breaks larger writes into
// * page-safe chunks.
// * ========================================================== */
//bool EEPROM_PageProgram(uint32_t addr, const volatile uint8_t *data, uint16_t len)
//{
//    uint16_t i;
//    uint8_t rx;
//    uint16_t page_offset;
//
//    if((data == NULL) || (len == 0U) || (len > FLASH_PAGE_SIZE))
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    page_offset = (uint16_t)(addr % FLASH_PAGE_SIZE);
//
//    if((uint32_t)page_offset + (uint32_t)len > FLASH_PAGE_SIZE)
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    if(!EEPROM_WriteEnable())
//    {
//        return false;
//    }
//
//    EEPROM_CS_LOW();
//
//    if(!SPI_TxRx(CMD_PP, &rx) || !SPI_TxRx((uint8_t)(addr >> 16U), &rx) ||
//       !SPI_TxRx((uint8_t)(addr >> 8U), &rx) || !SPI_TxRx((uint8_t)addr, &rx))
//    {
//        EEPROM_CS_HIGH();
//        return false;
//    }
//
//    for(i = 0U; i < len; i++)
//    {
//        if(!SPI_TxRx(data[i], &rx))
//        {
//            EEPROM_CS_HIGH();
//            return false;
//        }
//    }
//
//    EEPROM_CS_HIGH();
//    return Flash_WaitReady(FLASH_PROGRAM_TIMEOUT_POLLS);
//}
//
///* ============================================================
// * READ ONE BYTE
// * Existing project code expects a uint8_t return value, so on failure 0xFF is returned and the fault flag is set.
// * ========================================================== */
//uint8_t EEPROM_ReadByte(uint32_t addr)
//{
//    uint8_t rx;
//    uint8_t data;
//    EEPROM_CS_LOW();
//
//    if(!SPI_TxRx(CMD_READ, &rx) || !SPI_TxRx((uint8_t)(addr >> 16U), &rx) || !SPI_TxRx((uint8_t)(addr >> 8U), &rx) ||
//       !SPI_TxRx((uint8_t)addr, &rx) || !SPI_TxRx(0xFFU, &data))
//    {
//        EEPROM_CS_HIGH();
//        return 0xFFU;
//    }
//
//    EEPROM_CS_HIGH();
//    return data;
//}
//
///* ============================================================
// * READ PAGE / ARBITRARY NUMBER OF BYTES
// * ========================================================== */
//bool EEPROM_ReadPage(uint32_t addr, volatile uint8_t *buf, uint16_t len)
//{
//    uint16_t i;
//    uint8_t rx;
//
//    if((buf == NULL) || (len == 0U))
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    EEPROM_CS_LOW();
//
//    if(!SPI_TxRx(CMD_READ, &rx) || !SPI_TxRx((uint8_t)(addr >> 16U), &rx) ||
//       !SPI_TxRx((uint8_t)(addr >> 8U), &rx) || !SPI_TxRx((uint8_t)addr, &rx))
//    {
//        EEPROM_CS_HIGH();
//        return false;
//    }
//
//    for(i = 0U; i < len; i++)
//    {
//        if(!SPI_TxRx(0xFFU, (uint8_t *)&buf[i]))
//        {
//            EEPROM_CS_HIGH();
//            return false;
//        }
//    }
//
//    EEPROM_CS_HIGH();
//    return true;
//}
//
///* ============================================================
// * JEDEC ID
// * ========================================================== */
//bool Flash_ReadJedecId(uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity)
//{
//    uint8_t rx;
//
//    if((manufacturer == NULL) || (memory_type == NULL) || (capacity == NULL))
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//
//    EEPROM_CS_LOW();
//
//    if(!SPI_TxRx(CMD_RDID, &rx) || !SPI_TxRx(0xFFU, manufacturer) || !SPI_TxRx(0xFFU, memory_type) || !SPI_TxRx(0xFFU, capacity))
//    {
//        EEPROM_CS_HIGH();
//        return false;
//    }
//
//    EEPROM_CS_HIGH();
//    return true;
//}
//
///* ============================================================
// * FLASH PROBE
// * This intentionally does not hard-code a manufacturer because
// * the exact NOR flash part is not specified in the current code.
// * It rejects the common no-device responses 00 00 00 and FF FF FF.
// * ========================================================== */
//bool Flash_Probe(void)
//{
//    uint8_t manufacturer;
//    uint8_t memory_type;
//    uint8_t capacity;
//
//    if(!Flash_ReadJedecId(&manufacturer, &memory_type, &capacity))
//    {
//        return false;
//    }
//
//    if(((manufacturer == 0xFFU) && (memory_type  == 0xFFU) && (capacity     == 0xFFU)) ||
//       ((manufacturer == 0x00U) && (memory_type  == 0x00U) && (capacity     == 0x00U)))
//    {
//        g_flash_spi_fault = true;
//        return false;
//    }
//    return true;
//}
//
///* ============================================================
// * FAULT STATUS
// * ========================================================== */
//bool EEPROM_SPI_HasFault(void)
//{
//    return g_flash_spi_fault;
//}
//
//void EEPROM_SPI_ClearFault(void)
//{
//    g_flash_spi_fault = false;
//}

