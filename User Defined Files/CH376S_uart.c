#include <stdint.h>
#include <stdbool.h>
#include <CH376S_uart.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/ssi.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"

uint16_t usb_line_counter = 0;

char usb_pad1[] =
"\r\n                                                                 ";

char usb_pad2[] =
"                              \r\n                                   ";

#define USB_BLOCK_SIZE 512

static char usb_block[USB_BLOCK_SIZE];
extern uint16_t usb_block_index;
extern uint32_t g_sysClock;

/* ================= DELAY ================= */
void delay_ms(uint32_t ms)
{
    uint32_t cycles = g_sysClock / 3000;
    while(ms--) SysCtlDelay(cycles);
}

void CH376_Send(uint8_t b)
{
    UARTCharPut(UART7_BASE, b);
}
uint8_t CH376_Read(void)
{
    while(!UARTCharsAvail(UART7_BASE));
    return UARTCharGet(UART7_BASE);
}

bool CH376_Wait(uint32_t timeout)
{
    while(timeout--)
    {
        if(UARTCharsAvail(UART7_BASE)) return true;
        SysCtlDelay(100);
    }
    return false;
}

/* ===================== DRIVER ===================== */

void CH376_Reset(void)
{
    /* Send: 57 AB 05 */
    UARTCharPut(UART7_BASE, CMD1);
    UARTCharPut(UART7_BASE, CMD2);
    UARTCharPut(UART7_BASE, RESET_CMD);

    /* CH376 reboot time */
    delay_ms(200);

    /* Flush any garbage bytes from UART */
    while(UARTCharsAvail(UART7_BASE))
        UARTCharGet(UART7_BASE);
}

bool CH376_CheckExist(void)
{
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(CHECK_EXIST);
    CH376_Send(0x01);

    if(!CH376_Wait(50000)) return false;
    return (CH376_Read() == 0xFE);
}

bool CH376_SetUSBMode(void)
{
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(SET_USB_MODE);
    CH376_Send(0x06);

    if(!CH376_Wait(50000)) return false;

    uint8_t r1 = CH376_Read();
    uint8_t r2 = CH376_Read();

    return (r1 == 0x51 && r2 == 0x15);
}

bool CH376_DiskConnect(void)
{
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(DISK_CONNECT);

    if(!CH376_Wait(50000)) return false;
    return (CH376_Read() == 0x14);
}

bool CH376_Mount(void)
{
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(DISK_MOUNT);

    if(!CH376_Wait(80000)) return false;
    return (CH376_Read() == 0x14);
}

void CH376_SetFileName(const char *name)
{
    int i = 0;
    int j = 0;

    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(SET_FILE_NAME);

    CH376_Send(SET_FILE_NAME);   // required

    /* Send name part (max 8) */
    while(name[i] && name[i] != '.' && i < 8)
        CH376_Send(name[i++]);

    while(i < 8) CH376_Send(' ');   // pad

    /* Send extension */
    if(name[i] == '.') i++;
    for(j=0;j<3;j++)
    {
        if(name[i]) CH376_Send(name[i++]);
        else CH376_Send(' ');
    }

    CH376_Send(0x00);
    delay_ms(20);
}


bool CH376_CreateFile(void)
{
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(FILE_CREATE);

    if(!CH376_Wait(800000)) return false;
    return (CH376_Read() == 0x14);
}

bool CH376_Write(char *data)
{
    uint16_t len = 0;
    while(data[len]) len++;

    delay_ms(50);

    /* Tell length */
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(BYTE_WRITE);
    CH376_Send(len & 0xFF);
    CH376_Send(0x00);

    if(!CH376_Wait(80000)) return false;
    if(CH376_Read() != 0x1E) return false;

    delay_ms(10);   // IMPORTANT

    /* Send data */
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(WRITE_DATA);

    uint16_t i;
    for(i=0;i<len;i++)
        CH376_Send(data[i]);

    if(!CH376_Wait(80000)) return false;
    CH376_Read();   // consume 0x19

    delay_ms(10);   // IMPORTANT

    /* Update file size */
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(UPDATE_SIZE);

    if(!CH376_Wait(80000)) return false;
    CH376_Read();

    return true;
}

//Check
bool CH376_WriteLen(char *data, uint16_t len)
{
    while(len)
    {
        uint8_t chunk = (len > 255) ? 255 : len;

        CH376_Send(CMD1);
        CH376_Send(CMD2);
        CH376_Send(BYTE_WRITE);
        CH376_Send(chunk);
        CH376_Send(0x00);

        if(!CH376_Wait(80000)) return false;
        if(CH376_Read() != 0x1E) return false;

        CH376_Send(CMD1);
        CH376_Send(CMD2);
        CH376_Send(WRITE_DATA);

        for(uint8_t i=0;i<chunk;i++)
            CH376_Send(*data++);

        if(!CH376_Wait(80000)) return false;
        CH376_Read();

        /* UPDATE FILE SIZE AFTER EVERY WRITE */
        CH376_Send(CMD1);
        CH376_Send(CMD2);
        CH376_Send(UPDATE_SIZE);

        if(!CH376_Wait(80000)) return false;
        CH376_Read();

        len -= chunk;
    }

//    CH376_Send(CMD1);
//    CH376_Send(CMD2);
//    CH376_Send(UPDATE_SIZE);
//
//    if(!CH376_Wait(80000)) return false;
//    CH376_Read();

    return true;
}

void USB_BufferWrite(char *data, uint16_t len)
{
    for(uint16_t i = 0; i < len; i++)
    {
        usb_block[usb_block_index++] = data[i];

        if(usb_block_index >= USB_BLOCK_SIZE)
        {
            CH376_WriteLen(usb_block, USB_BLOCK_SIZE);
            usb_block_index = 0;
        }
    }
}

void USB_BufferFlush(void)
{
    if(usb_block_index > 0)
    {
        CH376_WriteLen(usb_block, usb_block_index);
        usb_block_index = 0;
    }
}
//

void CH376_Close(void)
{
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(FILE_CLOSE);
    CH376_Send(0x01);

    CH376_Wait(80000);
    CH376_Read();
}

bool CH376_OpenFile(void)
{
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(FILE_OPEN);

    if(!CH376_Wait(80000)) return false;
    return (CH376_Read() == 0x14);
}

bool CH376_AppendMode(void)
{
    /* Move file pointer to end */
    CH376_Send(CMD1);
    CH376_Send(CMD2);
    CH376_Send(BYTE_LOCATE);

    /* 0xFFFFFFFF → means END OF FILE */
    CH376_Send(0xFF);
    CH376_Send(0xFF);
    CH376_Send(0xFF);
    CH376_Send(0xFF);

    if(!CH376_Wait(80000)) return false;
    return (CH376_Read() == 0x14);
}

void USB_Write(void)
{
    CH376_Reset();
    if(!CH376_CheckExist()) while(1);
    if(!CH376_SetUSBMode()) while(1);
    if(!CH376_DiskConnect()) while(1);
    if(!CH376_Mount()) while(1);

//    CH376_Reset();
    CH376_SetFileName("EEEE    TXT");

    if(!CH376_OpenFile()){
        if(!CH376_CreateFile()) while(1);
    }

    if(!CH376_AppendMode()) while(1);

    CH376_Write("HELLO FROM USB\r\n");
    CH376_Close();
}
