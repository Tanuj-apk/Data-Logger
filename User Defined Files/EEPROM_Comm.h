#ifndef USER_DEFINED_FILES_EEPROM_COMM_H_
#define USER_DEFINED_FILES_EEPROM_COMM_H_

#include <stdint.h>
#include <stdbool.h>

#include "sd_spi.h"
#include "ff.h"

/* Flash log area */
#define FLASH_LOG_START              0x00000100UL
#define FLASH_LOG_END                0x0FFFFFF0UL

/** Fixed SPI-flash record.
 * Byte 0..3    EPOCH_TIME
 * Byte 4..7    REFERENCE_LOCATION / RFID
 * Byte 8..11   DISTANCE_FROM_REFERENCE_M
 * Byte 12..15  EVENT-SPECIFIC DATA
 * EVENT_ID IS NOT STORED IN FLASH. */
#define EVENT_LOG_FLASH_RECORD_SIZE  16U

/** CPU CAN event-record wire format.
 * Byte 0..1    EVENT_ID
 * Byte 2       SOURCE
 * Byte 3       FORMAT_TYPE
 * Byte 4       LOCATION_VALID
 * Byte 5..8    EPOCH_TIME
 * Byte 9..12   REFERENCE_LOCATION
 * Byte 13..16  DISTANCE_FROM_REFERENCE_M
 * Byte 17      PAYLOAD_LENGTH
 * Byte 18..    EVENT-SPECIFIC PAYLOAD */
#define EVENT_LOG_WIRE_HEADER_SIZE    18U
#define EVENT_LOG_SOURCE_ONBOARD      0U

#define EVENT_LOG_FMT_NONE                  0U
#define EVENT_LOG_FMT_SPAD_ONBOARD          1U
#define EVENT_LOG_FMT_SPAD_STATIONARY      2U
#define EVENT_LOG_FMT_HEAD_ON_ONBOARD      3U
#define EVENT_LOG_FMT_HEAD_ON_STATIONARY   4U
#define EVENT_LOG_FMT_REAR_END_ONBOARD     5U
#define EVENT_LOG_FMT_REAR_END_STATIONARY  6U
#define EVENT_LOG_FMT_LOCO_SOS             7U
#define EVENT_LOG_FMT_COMMUNICATION        8U

/** Event IDs are used only while interpreting the CPU CAN record.
 * They are NOT written to SPI flash. */
#define LOG_EVT_SPAD                           0x0201U
#define LOG_EVT_HEAD_ON_COLLISION              0x0202U
#define LOG_EVT_REAR_END_COLLISION             0x0203U
#define LOG_EVT_MANUAL_SOS                     0x0204U
#define LOG_EVT_MANUAL_SOS_CANCEL              0x0205U
#define LOG_EVT_STATIONARY_SOS                 0x0206U
#define LOG_EVT_STATIONARY_SOS_CANCEL          0x0207U
#define LOG_EVT_FOREIGN_RFID_SOS               0x0208U
#define LOG_EVT_ODO_ERROR_SOS                  0x0209U
#define LOG_EVT_SHUNT_LIMIT_SOS                0x020AU
#define LOG_EVT_TRAIN_TRIP                     0x020BU
#define LOG_EVT_UNUSUAL_STOP                   0x020CU
#define LOG_EVT_UNUSUAL_STOP_RELEASE           0x020DU

#define LOG_EVT_COMM_FAILURE                   0x0301U
#define LOG_EVT_COMM_FAILURE_RECOVERY          0x0302U
#define LOG_EVT_ADJACENT_KAVACH_COMM_FAILURE   0x0303U
#define LOG_EVT_ADJACENT_KAVACH_COMM_RECOVERY  0x0304U
#define LOG_EVT_TSRMS_COMM_FAILURE             0x0305U
#define LOG_EVT_TSRMS_COMM_RECOVERY            0x0306U
#define LOG_EVT_EI_COMM_FAILURE                0x0307U
#define LOG_EVT_EI_COMM_RECOVERY               0x0308U

/* FATFS objects */
extern FATFS fs;
extern FIL file;
extern UINT bw;
extern FRESULT res;

/* Persistent log state */
extern uint32_t eeprom_log_addr;
extern volatile bool eeprom_logging_enabled;

/* Time conversion / formatting */
void seconds_to_calendar(uint32_t seconds, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec);
int u32_to_dec(char *out, uint32_t v);

/* CAN-record processing */
bool EventLog_CompressRecord(const uint8_t *wire, uint8_t wire_length, uint8_t storage[EVENT_LOG_FLASH_RECORD_SIZE]);
bool Flash_AppendEventRecord(const uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE]);

uint16_t EventLog_FormatCsv(const uint8_t record[EVENT_LOG_FLASH_RECORD_SIZE], char *line, uint16_t line_size);

/* Export */
uint8_t EEPROM_to_SD(void);
uint8_t EEPROM_to_USB(void);
uint8_t EEPROM_to_USB_DIRECT(void);

/* Existing test API */
void SD_WRITE(void);

#endif /* USER_DEFINED_FILES_EEPROM_COMM_H_ */
