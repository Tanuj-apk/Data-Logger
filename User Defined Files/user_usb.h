/*
 * user_usb.h
 * USB Host MSC wrapper for EK-TM4C1294XL
 *
 * Basic funda: Wraps TivaWare USB MSC + FatFs into a simple API.
 * Call UserUSB_Task() in your main loop. No ISRs, no blocking delays.
 *
 * HARDWARE NOTE:
 * Short TP7 (USB ID pin) to GND before powering on.
 * TP7 must be shorted to GND for stable USB host operation.
 *
 * Author: HP
 * Date:   23-May-2026
 */

#ifndef USER_CODE_HEADER_USER_USB_H_
#define USER_CODE_HEADER_USER_USB_H_

#include <stdint.h>
#include <stdbool.h>
#include "usblib/host/usbhmsc.h"

extern int a;
extern uint32_t g_sysClock;
extern volatile bool g_bLogFileOpened;

/*
 * USB State Machine.
 *
 * Happy path:
 * NO_DEVICE -> DEVICE_ENUM -> DEVICE_READY -> NO_DEVICE (when yanked out)
 *
 * Error scene (User has to physically re-plug to fix these):
 * TIMEOUT_DEVICE    - Pendrive taking too long to wake up.
 * FILESYSTEM_ERROR  - FatFs choked (mount/open/write failed).
 * UNKNOWN_DEVICE    - Plugged in a mouse or something instead of a pendrive.
 * POWER_FAULT       - VBUS power drop. Check your hardware setup.
 */
typedef enum
{
    //
    // No device is present.
    //
    STATE_NO_DEVICE,

    //
    // Mass storage device is being enumerated.
    //
    STATE_DEVICE_ENUM,

    //
    // Mass storage device is ready.
    //
    STATE_DEVICE_READY,

    //
    // An unsupported device has been attached.
    //
    STATE_UNKNOWN_DEVICE,

    //
    // A mass storage device was connected but failed to ever report ready.
    //
    STATE_TIMEOUT_DEVICE,


    //
    // Filesystem mount/open failed.
    //
    STATE_FILESYSTEM_ERROR,

    //
    // A power fault has occurred.
    //
    STATE_POWER_FAULT
}
tState;




/* Public APIs */
void UserUSB_Init(void);
void UserUSB_Task(void);
bool UserUSB_Write(const uint8_t *pData, uint32_t ui32Length);
bool UserUSB_IsReady(void);
bool UserUSB_CloseLogFile(void);
tState UserUSB_GetState(void);
void UserUSB_OpenLogFile(void);


#endif /* USER_CODE_HEADER_USER_USB_H_ */
