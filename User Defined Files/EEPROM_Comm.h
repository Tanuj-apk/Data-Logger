#ifndef USER_DEFINED_FILES_EEPROM_COMM_H_
#define USER_DEFINED_FILES_EEPROM_COMM_H_

#include <stdint.h>
#include <stdbool.h>
/* ================= FATFS (SD) ================= */
#include "sd_spi.h"
#include "ff.h"

/* ================= FATFS OBJECTS ================= */
FATFS fs;
FIL file;
UINT bw;
FRESULT res;

volatile uint32_t cpu_time_sec = 0;
volatile uint32_t loco_id = 0;
volatile uint16_t speed = 0;
volatile uint32_t abs_loc = 0;
volatile uint8_t direction = 0;
volatile uint32_t last_rfid = 0;
volatile uint16_t tin = 0;
volatile uint8_t brake_type = 0;
volatile uint8_t system_fail = 0;

volatile uint8_t log_frame_buf[3][6];
volatile uint8_t log_frame_shadow[18];
volatile uint8_t log_frame_buf_Read[3][6];
volatile uint8_t log_frames_received = 0;
volatile bool log_message_ready = false;
extern uint16_t usb_block_index;

static const uint8_t days_in_month[12] =
{
    31,28,31,30,31,30,31,31,30,31,30,31
};

#define EEPROM_ADDR 0x50
//============ Date and Time conversion ============
#define GPS_EPOCH_YEAR 1980
#define GPS_EPOCH_DAY_OFFSET 5U
#define SECONDS_PER_DAY 86400UL

uint8_t eeprom_page[64];
uint32_t eeprom_log_addr = 0x0100;   // start address for log storage
volatile bool eeprom_logging_enabled = true;
char line[256];

static inline uint8_t is_leap_year(uint16_t year)
{
    return ((year % 4U == 0U && year % 100U != 0U) || (year % 400U == 0U));
}

void seconds_to_calendar(uint32_t seconds, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec);
int u32_to_dec(char *out, uint32_t v);
uint8_t EEPROM_to_SD(void);
uint8_t EEPROM_to_USB(void);
void decode_log_message(void);
void SD_WRITE(void);

#endif /* USER_DEFINED_FILES_EEPROM_COMM_H_ */
