#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"

#include "EEPROM_Comm.h"
#include "CH376S_uart.h"
#include "EEPROM_spi.h"
#include "user_usb.h"

/* ============================================================
 * Existing FATFS objects
 * ========================================================== */
FATFS fs;
FIL file;
UINT bw;
FRESULT res;

/* ============================================================
 * Persistent logger state
 * Definitions are owned by main.c in this project.
 * ========================================================== */
extern uint32_t eeprom_log_addr;
extern volatile bool eeprom_logging_enabled;

/* ============================================================
 * Date/time conversion
 * Keep the same epoch convention that the current TIVA project
 * already uses for seconds_to_calendar().
 * ========================================================== */
#define GPS_EPOCH_YEAR       1980U
#define GPS_EPOCH_DAY_OFFSET 5U
#define SECONDS_PER_DAY      86400UL

static const uint8_t days_in_month[12] =
{
    31U, 28U, 31U, 30U,
    31U, 30U, 31U, 31U,
    30U, 31U, 30U, 31U
};

/* ============================================================
 * Internal constants
 * ========================================================== */
#define FLASH_PAGE_SIZE      256U
#define EVENT_LOG_CSV_LINE_SIZE 256U

/* ============================================================
 * Byte helpers
 * ========================================================== */
static uint16_t GetU16LE(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8U);
}

static uint32_t GetU32LE(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static void PutU32LE(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8U) & 0xFFU);
    p[2] = (uint8_t)((value >> 16U) & 0xFFU);
    p[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

/* ============================================================
 * Date/time helpers
 * ========================================================== */
static uint8_t is_leap_year(uint16_t year)
{
    return (uint8_t)(((year % 4U == 0U) && (year % 100U != 0U)) || (year % 400U == 0U));
}

void seconds_to_calendar(uint32_t seconds, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec)
{
    uint32_t days;
    uint16_t y;
    uint8_t m;

    if((year == NULL) || (month == NULL) || (day == NULL) || (hour == NULL) || (min == NULL) || (sec == NULL))
    {
        return;
    }

    seconds += GPS_EPOCH_DAY_OFFSET * SECONDS_PER_DAY;
    *sec = (uint8_t)(seconds % 60U);

    seconds /= 60U;
    *min = (uint8_t)(seconds % 60U);

    seconds /= 60U;
    *hour = (uint8_t)(seconds % 24U);
    days = seconds / 24U;

    for(y = GPS_EPOCH_YEAR; ; y++)
    {
        uint16_t year_days = is_leap_year(y) ? 366U : 365U;
        if(days < year_days)
        {
            break;
        }
        days -= year_days;
    }

    *year = y;
    for(m = 1U; m <= 12U; m++)
    {
        uint8_t dim = days_in_month[m - 1U];
        if((m == 2U) && is_leap_year(*year))
        {
            dim++;
        }

        if(days < dim)
        {
            break;
        }
        days -= dim;
    }
    *month = m;
    *day = (uint8_t)(days + 1U);
}

int u32_to_dec(char *out, uint32_t v)
{
    char buf[10];
    int i = 0;
    int j = 0;

    if(out == NULL)
    {
        return 0;
    }

    if(v == 0U)
    {
        out[0] = '0';
        return 1;
    }

    while(v > 0U)
    {
        buf[i++] = (char)((v % 10U) + '0');
        v /= 10U;
    }

    while(i > 0)
    {
        out[j++] = buf[--i];
    }
    return j;
}

static int u8_to_hex2(char *out, uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    if(out == NULL)
    {
        return 0;
    }

    out[0] = hex[(value >> 4U) & 0x0FU];
    out[1] = hex[value & 0x0FU];
    return 2;
}

static int csv_put_u32(char *line, uint16_t *index, uint16_t line_size, uint32_t value)
{
    int written;
    written = u32_to_dec(&line[*index], value);
    if((written < 0) || ((*index + (uint16_t)written) >= line_size))
    {
        return 0;
    }

    *index = (uint16_t)(*index + (uint16_t)written);
    return 1;
}

static int csv_put_hex8(char *line, uint16_t *index, uint16_t line_size, uint8_t value)
{
    if((*index + 2U) >= line_size)
    {
        return 0;
    }

    *index = (uint16_t)(*index + (uint16_t)u8_to_hex2(&line[*index], value));
    return 1;
}

/* ============================================================
 * CPU wire record -> fixed 16-byte flash record
 * Flash:
 *   0..3   EPOCH_TIME
 *   4..7   REFERENCE_LOCATION
 *   8..11  DISTANCE_FROM_REFERENCE
 *   12..15 EVENT-SPECIFIC DATA
 * EVENT_ID is deliberately NOT copied to flash.
 * ========================================================== */
bool EventLog_CompressRecord(const uint8_t *wire, uint8_t wire_length, uint8_t storage[EVENT_LOG_FLASH_RECORD_SIZE])
{
    uint16_t event_id;
    uint8_t source;
    uint8_t format;
    uint8_t location_valid;
    uint8_t payload_length;
    const uint8_t *payload;

    if((wire == NULL) || (storage == NULL))
    {
        return false;
    }

    if(wire_length < EVENT_LOG_WIRE_HEADER_SIZE)
    {
        return false;
    }

    event_id = GetU16LE(&wire[0]);
    source = wire[2];
    format = wire[3];
    location_valid = wire[4];
    payload_length = wire[17];

    if((uint16_t)(EVENT_LOG_WIRE_HEADER_SIZE + payload_length) > wire_length)
    {
        return false;
    }

    /** This TIVA logger is connected to the onboard/loco CPU. */
    if(source != EVENT_LOG_SOURCE_ONBOARD)
    {
        return false;
    }

    memset(storage, 0, EVENT_LOG_FLASH_RECORD_SIZE);
    /** Common FRS fields.*/
    PutU32LE(&storage[0], GetU32LE(&wire[5]));

    if(location_valid != 0U)
    {
        PutU32LE(&storage[4], GetU32LE(&wire[9]));
        PutU32LE(&storage[8], GetU32LE(&wire[13]));
    }
    else
    {
        /** Invalid location marker. */
        PutU32LE(&storage[4], 0xFFFFFFFFUL);
        PutU32LE(&storage[8], 0U);
    }

    payload = &wire[18];

    /** The four specific bytes remain uniform for every event.
     *  The interpretation depends on the event that generated the record. EVENT_ID itself is not retained. */
    switch(format)
    {
        case EVENT_LOG_FMT_NONE:
        {
            if(payload_length != 0U)
            {
                return false;
            }
        }
        break;

        case EVENT_LOG_FMT_SPAD_ONBOARD:
        {
            /** 2-byte Stationary KAVACH ID
             * 1-byte Signal Type */
            if((event_id != LOG_EVT_SPAD) || (payload_length != 3U))
            {
                return false;
            }

            storage[12] = payload[0];
            storage[13] = payload[1];
            storage[14] = payload[2];
        }
        break;

        case EVENT_LOG_FMT_HEAD_ON_ONBOARD:
        {
            /** 2-byte Stationary KAVACH ID */
            if((event_id != LOG_EVT_HEAD_ON_COLLISION) || (payload_length != 2U))
            {
                return false;
            }

            storage[12] = payload[0];
            storage[13] = payload[1];
        }
        break;

        case EVENT_LOG_FMT_REAR_END_ONBOARD:
        {
            /** 2-byte L2 Onboard KAVACH ID */
            if((event_id != LOG_EVT_REAR_END_COLLISION) || (payload_length != 2U))
            {
                return false;
            }

            storage[12] = payload[0];
            storage[13] = payload[1];
        }
        break;

        case EVENT_LOG_FMT_LOCO_SOS:
        {
            /** 2-byte source L1 Onboard KAVACH ID */
            if((event_id != LOG_EVT_MANUAL_SOS) || (payload_length != 2U))
            {
                return false;
            }

            storage[12] = payload[0];
            storage[13] = payload[1];
        }
        break;

        case EVENT_LOG_FMT_COMMUNICATION:
        {
            /** These fields are not required in the onboard FRS logging output, but the CPU wire information is kept
             * compactly because four specific bytes are available:
             * byte 12:
             *   bits 1:0 = target
             *   bits 3:2 = channel
             *   bit  4   = peer_id_valid
             * byte 13..14 = peer ID, if valid
             * byte 15 = reserved
             */
            if((payload_length != 3U) && (payload_length != 5U))
            {
                return false;
            }

            storage[12] = (uint8_t)((payload[0] & 0x03U) | ((payload[1] & 0x03U) << 2U) | ((payload[2] != 0U) ? 0x10U : 0U));
            if(payload[2] != 0U)
            {
                if(payload_length != 5U)
                {
                    return false;
                }

                storage[13] = payload[3];
                storage[14] = payload[4];
            }
        }
        break;

        default:
            /** Stationary-only formats are not expected on this onboard CPU -> TIVA link. */
            return false;
    }

    return true;
}

/* ============================================================
 * Append one 16-byte record to SPI NOR flash
 * ========================================================== */
bool Flash_AppendEventRecord(const uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE])
{
    uint32_t addr;
    uint16_t remaining;
    uint16_t offset;

    if(record == NULL)
    {
        return false;
    }

    if(!eeprom_logging_enabled)
    {
        return false;
    }

    addr = eeprom_log_addr;

    /** Do not wrap and overwrite existing log records. */
    if((addr < FLASH_LOG_START) || ((addr + EVENT_LOG_FLASH_RECORD_SIZE) > FLASH_LOG_END))
    {
        eeprom_logging_enabled = false;
        return false;
    }

    remaining = EVENT_LOG_FLASH_RECORD_SIZE;
    offset = 0U;

    while(remaining > 0U)
    {
        uint32_t page_offset;
        uint16_t page_remaining;
        uint16_t chunk;

        page_offset = addr % FLASH_PAGE_SIZE;
        page_remaining = (uint16_t)(FLASH_PAGE_SIZE - page_offset);
        chunk = (remaining < page_remaining) ? remaining : page_remaining;

        EEPROM_PageProgram(addr, (volatile uint8_t *)&record[offset], chunk);
        addr += chunk;
        offset = (uint16_t)(offset + chunk);
        remaining = (uint16_t)(remaining - chunk);
    }
    eeprom_log_addr = addr;
    return true;
}

/* ============================================================
 * Convert one 16-byte flash record into CSV
 * No EVENT_ID is available in flash, so the four specific bytes are intentionally exported as raw hexadecimal bytes.
 * ========================================================== */
uint16_t EventLog_FormatCsv(const uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE], char *line, uint16_t line_size)
{
    uint32_t epoch;
    uint32_t reference_location;
    uint32_t distance;

    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    uint16_t idx = 0U;

    if((record == NULL) || (line == NULL) || (line_size < 4U))
    {
        return 0U;
    }

    epoch = GetU32LE(&record[0]);

    reference_location = GetU32LE(&record[4]);

    distance = GetU32LE(&record[8]);

    seconds_to_calendar(epoch, &year, &month, &day, &hour, &minute, &second);

    /** CSV:
     * Epoch,
     * Date,
     * Time,
     * Reference_Location_m,
     * Distance_From_Reference_m,
     * SpecificData0..3 */
    if(!csv_put_u32(line, &idx, line_size, epoch))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_u32(line, &idx, line_size, day))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = '/';

    if(!csv_put_u32(line, &idx, line_size, month))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = '/';

    if(!csv_put_u32(line, &idx, line_size, year))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_u32(line, &idx, line_size, hour))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ':';

    if(!csv_put_u32(line, &idx, line_size, minute))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ':';

    if(!csv_put_u32(line, &idx, line_size, second))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_u32(line, &idx, line_size, reference_location))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_u32(line, &idx, line_size, distance))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_hex8(line, &idx, line_size, record[12]))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_hex8(line, &idx, line_size, record[13]))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_hex8(line, &idx, line_size, record[14]))
    {
        return 0U;
    }

    if((idx + 1U) >= line_size) return 0U;
    line[idx++] = ',';

    if(!csv_put_hex8(line, &idx, line_size, record[15]))
    {
        return 0U;
    }

    if((idx + 2U) >= line_size)
    {
        return 0U;
    }

    line[idx++] = '\r';
    line[idx++] = '\n';
    return idx;
}

/* ============================================================
 * CSV header
 * ========================================================== */
static const char EVENT_LOG_CSV_HEADER[] =
    "Epoch,Date,Time,Reference_Location_m,"
    "Distance_From_Reference_m,"
    "SpecificData0,SpecificData1,"
    "SpecificData2,SpecificData3\r\n";

/* ============================================================
 * Read one flash record
 * ========================================================== */
static bool EventLog_ReadRecord(uint32_t addr, uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE])
{
    EEPROM_ReadPage(addr, record, EVENT_LOG_FLASH_RECORD_SIZE);
    return true;
}

/* ============================================================
 * SD dump
 * ========================================================== */
uint8_t EEPROM_to_SD(void)
{
    uint32_t addr;
    uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE];
    char line[EVENT_LOG_CSV_LINE_SIZE];
    uint16_t line_len;

    if(eeprom_log_addr <= FLASH_LOG_START)
    {
        return 0U;
    }

    if(SD_Init() != 0)
    {
        return 0U;
    }

    if(f_mount(&fs, "", 1) != FR_OK)
    {
        return 0U;
    }

    if(f_open(&file, "EVENTLOG.CSV", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
        return 0U;
    }

    if(f_write(&file, EVENT_LOG_CSV_HEADER, (UINT)strlen(EVENT_LOG_CSV_HEADER), &bw) != FR_OK)
    {
        f_close(&file);
        return 0U;
    }

    addr = FLASH_LOG_START;
    while((addr + EVENT_LOG_FLASH_RECORD_SIZE) <= eeprom_log_addr)
    {
        EventLog_ReadRecord(addr, record);
        line_len = EventLog_FormatCsv(record, line, sizeof(line));
        if(line_len == 0U)
        {
            f_close(&file);
            return 0U;
        }

        if(f_write(&file, line, line_len, &bw) != FR_OK)
        {
            f_close(&file);
            return 0U;
        }

        if(bw != line_len)
        {
            f_close(&file);
            return 0U;
        }

        addr += EVENT_LOG_FLASH_RECORD_SIZE;
    }

    f_sync(&file);
    f_close(&file);
    return 1U;
}

/* ============================================================
 * CH376 USB dump
 * ========================================================== */
uint8_t EEPROM_to_USB(void)
{
    uint32_t addr;
    uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE];
    char line[EVENT_LOG_CSV_LINE_SIZE];
    uint16_t line_len;

    if(eeprom_log_addr <= FLASH_LOG_START)
    {
        return 0U;
    }

    CH376_Reset();

    if(!CH376_CheckExist())
    {
        return 0U;
    }

    if(!CH376_SetUSBMode())
    {
        return 0U;
    }

    if(!CH376_DiskConnect())
    {
        return 0U;
    }

    if(!CH376_Mount())
    {
        return 0U;
    }

    CH376_SetFileName("EVENTLOG.CSV");

    if(!CH376_OpenFile())
    {
        if(!CH376_CreateFile())
        {
            return 0U;
        }
    }

    if(!CH376_AppendMode())
    {
        return 0U;
    }

    /** Header. */
    USB_BufferWrite((char *)EVENT_LOG_CSV_HEADER, (uint16_t)strlen(EVENT_LOG_CSV_HEADER));
    USB_BufferFlush();
    addr = FLASH_LOG_START;
    while((addr + EVENT_LOG_FLASH_RECORD_SIZE) <= eeprom_log_addr)
    {
        EventLog_ReadRecord(addr, record);
        line_len = EventLog_FormatCsv(record, line, sizeof(line));

        if(line_len == 0U)
        {
            CH376_Close();
            return 0U;
        }

        USB_BufferWrite(line, line_len);
        addr += EVENT_LOG_FLASH_RECORD_SIZE;
    }

    USB_BufferFlush();
    CH376_Close();
    return 1U;
}

/* ============================================================
 * Native USB dump
 * The existing user_usb.c opens DATALOG.CSV when the drive becomes
 * ready, so this function writes the exported CSV to that file.
 * ========================================================== */
uint8_t EEPROM_to_USB_DIRECT(void)
{
    uint32_t addr;
    uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE];
    char line[EVENT_LOG_CSV_LINE_SIZE];
    uint16_t line_len;

    if(!UserUSB_IsReady())
    {
        return 0U;
    }

    if(!UserUSB_Write((const uint8_t *)EVENT_LOG_CSV_HEADER, strlen(EVENT_LOG_CSV_HEADER)))
    {
        return 0U;
    }

    addr = FLASH_LOG_START;
    while((addr + EVENT_LOG_FLASH_RECORD_SIZE) <= eeprom_log_addr)
    {
        UserUSB_Task();
        EventLog_ReadRecord(addr, record );
        line_len = EventLog_FormatCsv(record, line, sizeof(line));
        if(line_len == 0U)
        {
            return 0U;
        }

        if(!UserUSB_Write((const uint8_t *)line, line_len))
        {
            return 0U;
        }

        UserUSB_Task();
        addr += EVENT_LOG_FLASH_RECORD_SIZE;
    }
    return 1U;
}

/* ============================================================
 * Existing SD write test
 * ========================================================== */
void SD_WRITE(void)
{
    if(SD_Init() != 0)
    {
        return;
    }

    res = f_mount(&fs, "", 1);

    if(res != FR_OK)
    {
        return;
    }

    res = f_open(&file, "VEX.TXT", FA_OPEN_ALWAYS | FA_WRITE);

    if(res != FR_OK)
    {
        return;
    }

    res = f_lseek(&file, f_size(&file));
    if(res != FR_OK)
    {
        f_close(&file);
        return;
    }

    {
        const char msg[] = "HELLO FROM TI\r\n";

        res = f_write(&file, msg, (UINT)(sizeof(msg) - 1U), &bw);
    }

    f_sync(&file);
    f_close(&file);
    SysCtlDelay(SysCtlClockGet() / 3U);
}
