#ifndef USER_DEFINED_FILES_CH376S_UART_H_
#define USER_DEFINED_FILES_CH376S_UART_H_

#include <stdint.h>

/////////////////////////////////////////////////////
//////////////////// CH376 USB //////////////////////
/////////////////////////////////////////////////////
#define CMD1 0x57
#define CMD2 0xAB
#define CHECK_EXIST   0x06
#define SET_USB_MODE  0x15
#define DISK_CONNECT  0x30
#define DISK_MOUNT    0x31
#define SET_FILE_NAME 0x2F
#define FILE_CREATE   0x34
#define BYTE_WRITE    0x3C
#define WRITE_DATA    0x2D
#define UPDATE_SIZE   0x3D
#define FILE_CLOSE    0x36
#define RESET_CMD   0x05
#define FILE_OPEN     0x32
#define BYTE_LOCATE   0x39

void delay_ms(uint32_t ms);
void CH376_Send(uint8_t b);
uint8_t CH376_Read(void);
bool CH376_Wait(uint32_t timeout);
void CH376_Reset(void);
bool CH376_CheckExist(void);
bool CH376_SetUSBMode(void);
bool CH376_DiskConnect(void);
bool CH376_Mount(void);
void CH376_SetFileName(const char *name);
bool CH376_CreateFile(void);
bool CH376_Write(char *data);
bool CH376_WriteLen(char *data, uint16_t len);
void USB_BufferWrite(char *data, uint16_t len);
void USB_BufferFlush(void);
void CH376_Close(void);
bool CH376_OpenFile(void);
bool CH376_AppendMode(void);
void USB_Write(void);

#endif /* USER_DEFINED_FILES_CH376S_UART_H_ */
