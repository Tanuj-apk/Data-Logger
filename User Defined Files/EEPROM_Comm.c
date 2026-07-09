#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include <EEPROM_Comm.h>
#include <CH376S_uart.h>
#include "EEPROM_spi.h"
#include "user_usb.h"

uint8_t TEST_USB_DIRECT(void);

void seconds_to_calendar(uint32_t seconds, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec)
{
    uint32_t days;
    uint16_t y;
    uint8_t m;

    seconds += (GPS_EPOCH_DAY_OFFSET * SECONDS_PER_DAY);

    *sec = seconds % 60U;
    seconds /= 60U;
    *min = seconds % 60U;
    seconds /= 60U;
    *hour = seconds % 24U;
    days = seconds / 24U;

    for (y = GPS_EPOCH_YEAR;; y++)
    {
        uint16_t year_days = is_leap_year(y) ? 366U : 365U;
        if (days < year_days) break;
        days -= year_days;
    }
    *year = y;

    for (m = 1; m <= 12; m++)
    {
        uint8_t dim = days_in_month[m - 1];
        if (m == 2 && is_leap_year(*year)) dim++;
        if (days < dim) break;
        days -= dim;
    }
    *month = m;
    *day = (uint8_t)(days + 1U);
}

int u32_to_dec(char *out, uint32_t v)
{
    char buf[10];
    int i = 0, j = 0;

    if (v == 0) { out[0] = '0'; return 1; }

    while (v > 0)
    {
        buf[i++] = (v % 10) + '0';
        v /= 10;
    }
    while (i) out[j++] = buf[--i];
    return j;
}

uint8_t EEPROM_to_SD(void)
{
    uint16_t total_bytes = (eeprom_log_addr - 0x0100) & ~0x07;
    if (total_bytes == 0) return 0;

    if (SD_Init() != 0) return 0;
    if (f_mount(&fs, "", 1) != FR_OK) return 0;
    if (f_open(&file, "EEPROM.TXT", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return 0;

    f_lseek(&file, f_size(&file));

    uint32_t addr = 0x0100;

    while (addr < eeprom_log_addr)
    {
        /* Read 3 frames (1 complete log message = 24 bytes) */
        EEPROM_ReadPage(addr,     log_frame_buf_Read[0], 6);
        EEPROM_ReadPage(addr + 6, log_frame_buf_Read[1], 6);
        EEPROM_ReadPage(addr + 12, log_frame_buf_Read[2], 4);

        decode_log_message();

        /* Convert seconds → calendar */
        uint16_t year;
        uint8_t month, day, hour, min, sec;
        seconds_to_calendar(cpu_time_sec, &year, &month, &day, &hour, &min, &sec);

        //char line[256];
        uint8_t idx = 0;
        /* Date DD/MM/YYYY */
        idx += u32_to_dec(&line[idx], day);   line[idx++] = '/';
        idx += u32_to_dec(&line[idx], month); line[idx++] = '/';
        idx += u32_to_dec(&line[idx], year);  line[idx++] = ',';

        /* Time HH:MM:SS */
        idx += u32_to_dec(&line[idx], hour);  line[idx++] = ':';
        idx += u32_to_dec(&line[idx], min);   line[idx++] = ':';
        idx += u32_to_dec(&line[idx], sec);   line[idx++] = ',';

        /* CSV Fields */
        idx += u32_to_dec(&line[idx], loco_id);    line[idx++] = ',';
        idx += u32_to_dec(&line[idx], speed);      line[idx++] = ',';
        idx += u32_to_dec(&line[idx], abs_loc);    line[idx++] = ',';
        idx += u32_to_dec(&line[idx], direction);  line[idx++] = ',';
        idx += u32_to_dec(&line[idx], last_rfid);  line[idx++] = ',';
        idx += u32_to_dec(&line[idx], tin);        line[idx++] = ',';
        idx += u32_to_dec(&line[idx], brake_type); line[idx++] = ',';
        idx += u32_to_dec(&line[idx], system_fail);

        line[idx++] = '\r';
        line[idx++] = '\n';
        f_write(&file, line, idx, &bw);
        addr += 16;   // move to next complete log message
    }
    f_sync(&file);
    f_close(&file);
    return 1;
}

uint8_t EEPROM_to_USB(void)
{
    uint16_t total_bytes = (eeprom_log_addr - 0x0100) & ~0x07;
    if (total_bytes == 0) return 0;

    CH376_Reset();
    if(!CH376_CheckExist()) return 0;
    if(!CH376_SetUSBMode()) return 0;
    if(!CH376_DiskConnect()) return 0;
    if(!CH376_Mount()) return 0;

    CH376_SetFileName("EEPROM  TXT");

    if(!CH376_OpenFile())
        if(!CH376_CreateFile()) return 0;

    if(!CH376_AppendMode()) return 0;

    uint16_t addr = 0x0100;

    usb_block_index = 0;

    while (addr < eeprom_log_addr)
    {
        /* Read 3 frames (1 complete log message) */
        EEPROM_ReadPage(addr,     log_frame_buf_Read[0], 6);
        EEPROM_ReadPage(addr + 6, log_frame_buf_Read[1], 6);
        EEPROM_ReadPage(addr + 12, log_frame_buf_Read[2], 4);

        decode_log_message();

        uint16_t year;
        uint8_t month, day, hour, min, sec;

        seconds_to_calendar(cpu_time_sec, &year, &month, &day, &hour, &min, &sec);

//        char line[256];
        uint8_t idx = 0;
        idx += u32_to_dec(&line[idx], day);   line[idx++] = '/';
        idx += u32_to_dec(&line[idx], month); line[idx++] = '/';
        idx += u32_to_dec(&line[idx], year);  line[idx++] = ',';

        idx += u32_to_dec(&line[idx], hour);  line[idx++] = ':';
        idx += u32_to_dec(&line[idx], min);   line[idx++] = ':';
        idx += u32_to_dec(&line[idx], sec);   line[idx++] = ',';

        idx += u32_to_dec(&line[idx], loco_id);    line[idx++] = ',';
        idx += u32_to_dec(&line[idx], speed);      line[idx++] = ',';
        idx += u32_to_dec(&line[idx], abs_loc);    line[idx++] = ',';
        idx += u32_to_dec(&line[idx], direction);  line[idx++] = ',';
        idx += u32_to_dec(&line[idx], last_rfid);  line[idx++] = ',';
        idx += u32_to_dec(&line[idx], tin);        line[idx++] = ',';
        idx += u32_to_dec(&line[idx], brake_type); line[idx++] = ',';
        idx += u32_to_dec(&line[idx], system_fail);

        line[idx++] = '\r';
        line[idx++] = '\n';
        line[idx] = 0;

//        /* ---- UART Echo ---- */
//        for(uint16_t i = 0; i < idx; i++)
//        {
//            UARTCharPut(UART2_BASE, line[i]);
//        }

        USB_BufferWrite(line, idx);
        addr += 16;
    }
    USB_BufferFlush();
    CH376_Close();
    return 1;
}

uint8_t EEPROM_to_USB_DIRECT(void)
{
    if(!UserUSB_IsReady())
        return 0;

    static const char *msgs[] =
    {
        "====================================\r\n",
        "DIRECT USB DUMP TEST\r\n",
        "TM4C1294 Native USB Host\r\n",
        "1234567890\r\n",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n",
        "====================================\r\n"
    };

    for(uint32_t i = 0; i < (sizeof(msgs) / sizeof(msgs[0])); i++)
    {
        /* Service USB host stack */
        UserUSB_Task();

        if(!UserUSB_Write((const uint8_t *)msgs[i], strlen(msgs[i])))
        {
            UserUSB_CloseLogFile();
            return 0;
        }

        /* Service USB host stack again */
        UserUSB_Task();

        /* Small delay so USB host can complete transactions */
        SysCtlDelay(g_sysClock / 30000);   // ~100 us @120 MHz
    }

    /* Final service */
    UserUSB_Task();

//    UserUSB_CloseLogFile();
    TEST_USB_DIRECT();

    return 1;
}
uint8_t TEST_USB_DIRECT(void)
{
    uint16_t total_bytes = (eeprom_log_addr - 0x0100) & ~0x07;

    if(total_bytes == 0)
        return 0;

    /* USB Host must already be ready */
    if(!UserUSB_IsReady())
        return 0;

    uint16_t addr = 0x0100;

    while(addr < eeprom_log_addr)
    {
        /* Keep USB Host stack alive */
        UserUSB_Task();
        /* Read one complete log message from EEPROM */
        EEPROM_ReadPage(addr,     log_frame_buf_Read[0], 6);
        EEPROM_ReadPage(addr + 6, log_frame_buf_Read[1], 6);
        EEPROM_ReadPage(addr + 12,log_frame_buf_Read[2], 4);
        decode_log_message();

        uint16_t year;
        uint8_t month, day, hour, min, sec;
        seconds_to_calendar(cpu_time_sec, &year, &month, &day, &hour, &min, &sec);

        uint8_t idx = 0;
        /* Date */
        idx += u32_to_dec(&line[idx], day);
        line[idx++] = '/';

        idx += u32_to_dec(&line[idx], month);
        line[idx++] = '/';

        idx += u32_to_dec(&line[idx], year);
        line[idx++] = ',';

        /* Time */
        idx += u32_to_dec(&line[idx], hour);
        line[idx++] = ':';

        idx += u32_to_dec(&line[idx], min);
        line[idx++] = ':';

        idx += u32_to_dec(&line[idx], sec);
        line[idx++] = ',';

        /* CSV Data */
        idx += u32_to_dec(&line[idx], loco_id);
        line[idx++] = ',';

        idx += u32_to_dec(&line[idx], speed);
        line[idx++] = ',';

        idx += u32_to_dec(&line[idx], abs_loc);
        line[idx++] = ',';

        idx += u32_to_dec(&line[idx], direction);
        line[idx++] = ',';

        idx += u32_to_dec(&line[idx], last_rfid);
        line[idx++] = ',';

        idx += u32_to_dec(&line[idx], tin);
        line[idx++] = ',';

        idx += u32_to_dec(&line[idx], brake_type);
        line[idx++] = ',';

        idx += u32_to_dec(&line[idx], system_fail);

        line[idx++] = '\r';
        line[idx++] = '\n';

        /* Write one CSV line */
        if(!UserUSB_Write((const uint8_t *)line, idx))
        {
//            UserUSB_CloseLogFile();
            return 0;
        }

        /* Let USB Host process pending transfers */
        UserUSB_Task();

        addr += 16;
    }

    /* Flush and close once after entire dump */
//    UserUSB_CloseLogFile();

    return 1;
}
/////////////////////////////////////////////////////
//////////////////// MAIN ///////////////////////////
/////////////////////////////////////////////////////
void decode_log_message(void)
{
    uint8_t *f0, *f1, *f2;
    if(eeprom_logging_enabled)
    {
        f0 = (uint8_t*)log_frame_buf[0];
        f1 = (uint8_t*)log_frame_buf[1];
        f2 = (uint8_t*)log_frame_buf[2];
    }
    else
    {
        f0 = (uint8_t*)log_frame_buf_Read[0];
        f1 = (uint8_t*)log_frame_buf_Read[1];
        f2 = (uint8_t*)log_frame_buf_Read[2];
    }

    /* -------- CPU_TIME_SEC -------- */
    cpu_time_sec = ((uint32_t)f0[0] << 24) | ((uint32_t)f0[1] << 16) | ((uint32_t)f0[2] << 8) | ((uint32_t)f0[3]);

    /* -------- LocoID (20 bits) -------- */
    loco_id = ((uint32_t)f0[4] << 12) | ((uint32_t)(f0[5] >> 4) << 8) | ((uint32_t)(f0[5] & 0x0F) << 4) | ((uint32_t)(f1[0] >> 4));

    /* -------- Speed (9 bits) -------- */
    speed = ((uint16_t)(f1[0] & 0x0F) << 5) | ((uint16_t)(f1[1] >> 3));

    /* -------- Absolute Location (23 bits) -------- */
    abs_loc = ((uint32_t)(f1[1] & 0x07) << 20) | ((uint32_t)f1[2] << 12) | ((uint32_t)f1[3] << 4) | ((uint32_t)(f1[4] >> 4));

    /* -------- Direction -------- */
    direction = (f1[4] >> 3) & 0x01;

    /* -------- Last RFID (23 bits) -------- */
    last_rfid = ((uint32_t)(f1[4] & 0x07) << 20) | ((uint32_t)f1[5] << 12) | ((uint32_t)f2[0] << 4) | ((uint32_t)(f2[1] >> 4));

    /* -------- TIN (9 bits) -------- */
    tin = ((uint16_t)((f2[1] & 0x0F) << 5)) | ((uint16_t)((f2[2] & 0xF8) >> 3));

    /* -------- Brake Type -------- */
    brake_type = (f2[2] & 0x07);

    /* -------- System Failure -------- */
    system_fail = f2[3];
}

/////////////////////////////////////////////////////
//////////////////// SD CARD ////////////////////////
/////////////////////////////////////////////////////
void SD_WRITE(void)
{
    if (SD_Init() != 0)
        while(1);

    res = f_mount(&fs, "", 1);
    if (res != FR_OK) while (1);

    res = f_open(&file, "VEX.TXT", FA_OPEN_ALWAYS | FA_WRITE);
    if (res != FR_OK) while (1);

    res = f_lseek(&file, f_size(&file));
    if (res != FR_OK) while (1);

    char msg[] = "HELLO FROM TI\r\n";
    res = f_write(&file, msg, sizeof(msg)-1, &bw);
    if (res != FR_OK || bw == 0) while (1);

    f_sync(&file);
    f_close(&file);
    SysCtlDelay(SysCtlClockGet() / 3);
}
