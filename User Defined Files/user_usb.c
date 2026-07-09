/*
 * user_usb.c
 *
 * USB Host MSC Driver Module
 * Board: EK-TM4C1294XL | SDK: TivaWare 2.2.0.295 | FatFs: R0.09b
 *
 * ARCHITECTURE FUNDAS (Keep these in mind):
 * 1. Pure polling based. No RTOS threads or interrupt  here.
 * 2. USBOTGMain() is the boss. It must run every superloop.
 * 3. File stays open: We open DATALOG.CSV once on plug-in. f_sync() does the
 * heavy lifting after each write. f_close() only happens on explicit close or error.
 * 4. Disconnect scene: TivaWare destroys the USB pipes BEFORE firing MSC_EVENT_CLOSE.
 * Do NOT call f_close() there, or you'll crash trying to write to dead hardware.
 * 5. Non-blocking retries: TI's original code used SysCtlDelay (blocking). We use
 * SysTick for 500ms retries so the USB stack doesn't choke.
 * 6. Error handling: Any FatFs error throws STATE_FILESYSTEM_ERROR. User has to
 * physically re-plug. No auto-remounts, we don't want infinite retry loops.
 */

/* TODO:
 * Add StringFromFresult() helper for readable FatFs error messages.
 */

#include "user_usb.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "usblib/usblib.h"
#include "usblib/usbmsc.h"
#include "usblib/host/usbhost.h"
#include "usblib/host/usbhmsc.h"
#include "driverlib/sysctl.h"
#include "ff.h"
//#include "utils/uartstdio.h"

/* Max path size. Kept 80 matching TI code, sorted for future subdirs. */
#define PATH_BUF_SIZE   80

/* Try 4 times, 500ms each = 2 sec timeout. */
#define USBMSC_DRIVE_RETRY      40

/* SysTick stuff coming from usb_host_msc.c */
extern volatile uint32_t g_ui32SysTickCount;
extern uint32_t GetTickms(void);

/* Main MSC instance handle.
 * CANNOT be static. fat_usbmsc.c uses this directly for diskio ops.
 * Making it static will throw a linker error. */
tUSBHMSCInstance *g_psMSCInstance = 0;

/*
 * State tracking. Volatile coz they update in callbacks but read in main loop.
 * g_eUIState tracks last state so we don't spam UART prints.
 */
volatile tState g_eState = STATE_NO_DEVICE;
volatile tState g_eUIState = STATE_NO_DEVICE;

/* Enumeration retry counters */
static volatile uint32_t g_ui32DriveTimeout = USBMSC_DRIVE_RETRY;
static volatile uint32_t g_ui32LastDriveCheck = 0;

/* FatFs working memory. Must stay in scope. */
static FATFS g_sFatFs;
static DIR g_sDirObject;
static char g_cCwdBuf[PATH_BUF_SIZE] = "1:/";
static volatile bool g_bNeedUnmount = false;

/* Log file state. Volatile coz MSC_EVENT_CLOSE callback modifies it. */
static FIL g_sLogFile;
volatile bool g_bLogFileOpened = false;

/* Save unknown class code for UART prints */
static uint32_t g_ui32UnknownClass;

/* Prototypes */
void UserUSB_OpenLogFile(void);
static void MSCCallback(tUSBHMSCInstance *ps32Instance, uint32_t ui32Event, void *pvData);

/*
 * MSCCallback()
 * Fired from USBOTGMain() context (main loop, not ISR).
 */
static void MSCCallback(tUSBHMSCInstance *ps32Instance, uint32_t ui32Event, void *pvData)
{

//UARTprintf("MSC Event = %u\r\n", ui32Event);
    switch(ui32Event)
    {
        // Pendrive plugged in. Start enumeration.
        case MSC_EVENT_OPEN:
        {
            g_eState = STATE_DEVICE_ENUM;

            g_ui32DriveTimeout = USBMSC_DRIVE_RETRY;

            g_ui32LastDriveCheck = g_ui32SysTickCount;

            break;
        }


        case MSC_EVENT_CLOSE:
        {
            g_bNeedUnmount = true;

            // Pendrive yanked out. USB pipes are already dead.
            // DO NOT call f_close() or any FatFs stuff here. Just reset flags.
            g_bLogFileOpened = false;

            g_eState = STATE_NO_DEVICE;

            break;
        }

        default:
        {
            break;
        }
    }

} //function ends here




/*
 * USBHCDEvents()
 * Handles generic USB events (like someone plugging a mouse or a power fault).
 */
void USBHCDEvents(void *pvData)
{
    tEventInfo *pEventInfo;

    //
    // Cast this pointer to its actual type.
    //
    pEventInfo = (tEventInfo *)pvData;

    switch(pEventInfo->ui32Event)
    {
        //
        // Unknown device detected.
        //
        case USB_EVENT_UNKNOWN_CONNECTED:
        {
            //
            // Save the unknown class.
            //
            g_ui32UnknownClass = pEventInfo->ui32Instance;

            //
            // An unknown device was detected.
            //
            g_eState = STATE_UNKNOWN_DEVICE;

            break;
        }

        //
        // Keyboard has been unplugged.
        //
        case USB_EVENT_DISCONNECTED:
        {
            //
            // Unknown device has been removed.
            //
            g_eState = STATE_NO_DEVICE;

            break;
        }

        case USB_EVENT_POWER_FAULT:
        {
            //
            // No power means no device is present.
            //
            g_eState = STATE_POWER_FAULT;

            break;
        }

        default:
        {
            break;
        }
    }
} //function ends here




/*
 * UserUSB_Init()
 * Call this AFTER TI's USB stack init sequence in main().
 */
void UserUSB_Init(void)
{
    // Clean slate for warm reboots
    g_eState = STATE_NO_DEVICE;
    g_eUIState = STATE_NO_DEVICE;
    g_bLogFileOpened = false;

    g_ui32DriveTimeout = USBMSC_DRIVE_RETRY;
    g_ui32LastDriveCheck = 0;

    // Open MSC driver, returns handle to g_psMSCInstance.
    g_psMSCInstance = USBHMSCDriveOpen(0, MSCCallback);

} //function ends here



/*
 * UserUSB_Task()
 * Run this every superloop iteration. Non-blocking.
 */
int a = 0;
void UserUSB_Task(void)
{
    tState eStateCopy;
    FRESULT fresult;

// if unmount need to be done
    if(g_bNeedUnmount)
    {
        g_bNeedUnmount = false;
        (void)f_mount(NULL, "1:", 0);  // clears FatFs[0], forces clean remount next time
    }

    // 1. Service the USB stack. Keep this running fast (~5ms gaps max).
    USBOTGMain(GetTickms());


    // 2. Poll for MSC drive readiness during enumeration.
    if(g_eState == STATE_DEVICE_ENUM)
    {
        //
        // Take it easy on the Mass storage device if it is slow to
        // start up after connecting.
        //
        if(USBHMSCDriveReady(g_psMSCInstance) != 0)
        {
            //
            // Retry only every 500ms without blocking
            //
            if((g_ui32SysTickCount - g_ui32LastDriveCheck) >= 50)
            {
                //
                // Save retry timestamp
                //
                g_ui32LastDriveCheck = g_ui32SysTickCount;


                if(g_ui32DriveTimeout > 0)
                {
                    g_ui32DriveTimeout--;

                    if(g_ui32DriveTimeout == 0)
                    {
                        g_eState = STATE_TIMEOUT_DEVICE;
                    }
                }
            }
        }

        else
        {

            //
            // Reset the working directory to the root.
            //
//            g_cCwdBuf[0] = '/';
//            g_cCwdBuf[1] = '\0';
            g_cCwdBuf[0] = '1';
            g_cCwdBuf[1] = ':';
            g_cCwdBuf[2] = '/';
            g_cCwdBuf[3] = '\0';

            //
            // Attempt to open the directory. Some drives take longer to
            // start up than others, and this may fail even though the USB
            // device has enumerated.
            //
            fresult = f_mount(&g_sFatFs, "1:", 1);

            if(fresult != FR_OK)
            {
//                UARTprintf("Mount failed: %d\n", fresult);

                g_eState = STATE_FILESYSTEM_ERROR;
            }
            else
            {
                fresult = f_opendir(&g_sDirObject, g_cCwdBuf);

                if(fresult == FR_OK)
                {
                    g_eState = STATE_DEVICE_READY;
                }
                else
                {
//                    UARTprintf("Directory open failed: %d\n", fresult);

                    g_eState = STATE_FILESYSTEM_ERROR;
                }
            }
        }
    }


    //
    // See if the state has changed.  We make a copy of g_eUIState to
    // prevent a compiler warning about undefined order of volatile
    // accesses.
    //
    eStateCopy = g_eUIState;
    if(g_eState != eStateCopy)
    {
        //
        // Determine the new state.
        //
        switch(g_eState)
        {
            //
            // A previously connected device has been disconnected.
            //
            case STATE_NO_DEVICE:
            {
                if(g_eUIState == STATE_UNKNOWN_DEVICE)
                {
                    a = 1;
//                    UARTprintf("\nUnknown device disconnected.\n");
                }
                else
                {
                    a = 2;
//                    UARTprintf("\nUSB device disconnected.\n");
                }
                break;
            }


            //
            // A mass storage device is being enumerated.
            //
            case STATE_DEVICE_ENUM:
            {
                a = 3;
//                UARTprintf("USB Enumerating\n");
                break;
            }


            //
            // A mass storage device has been enumerated and initialized.
            //
            case STATE_DEVICE_READY:
            {
                a = 4;
//                UARTprintf("\nUSB device connected.\n");
                UserUSB_OpenLogFile();

                break;
            }



            //
            // An unknown device has been connected.
            //
            case STATE_UNKNOWN_DEVICE:
            {
                a = 5;
//                UARTprintf("Unknown Device Class (0x%02x) Connected.\n",
//                           g_ui32UnknownClass);
                break;
            }


            //
            // The connected mass storage device is not reporting ready.
            //
            case STATE_TIMEOUT_DEVICE:
            {
                //
                // If this is the first time in this state then print a
                // message.
                //
                a = 6;
//                UARTprintf("Device Timeout.\n");
                break;
            }

            //
            // Filesystem mount/open failure.
            //
            case STATE_FILESYSTEM_ERROR:
            {
                a = 7;
//                UARTprintf("Filesystem Error.\n");
                break;
            }

            //
            // A power fault has occurred.
            //
            case STATE_POWER_FAULT:
            {
                a =8;
//                UARTprintf("\nPower fault.\n");
                break;
            }
        }

        //
        // Save the current state.
        //
        g_eUIState = g_eState;
    }


} //function ends here



/*
 * UserUSB_OpenLogFile()
 * Opens DATALOG.CSV. Marks ready only if open, seek, and sync all pass.
 */
void UserUSB_OpenLogFile(void)
{
    FRESULT fresult;

    //
    // Prevent reopening
    //
    if(g_bLogFileOpened == true)
    {
        return;
    }

    //
    // Open existing file or create new file
    //
//    fresult = f_open(&g_sLogFile,
//                     "DATALOG.CSV",
//                     FA_OPEN_ALWAYS | FA_WRITE);
//    fresult = f_open(&g_sLogFile,
//           "1:/DATALOG.CSV",
//           FA_OPEN_ALWAYS | FA_WRITE);
    fresult = f_open(&g_sLogFile,
           "1:/DATALOG.CSV",
           FA_CREATE_ALWAYS | FA_WRITE);

    if(fresult != FR_OK)
    {
//        UARTprintf("File open failed: %d\n", fresult);

        g_eState = STATE_FILESYSTEM_ERROR;

        return;
    }

    //
    // Move file pointer to end for append operation
    //
    fresult = f_lseek(&g_sLogFile,
                      f_size(&g_sLogFile));

    if(fresult != FR_OK)
    {
//        UARTprintf("File seek failed: %d\n", fresult);

        (void)f_close(&g_sLogFile);

        g_eState = STATE_FILESYSTEM_ERROR;

        return;
    }

    //
    // Flush filesystem metadata
    //
    fresult = f_sync(&g_sLogFile);

    if(fresult != FR_OK)
    {
//        UARTprintf("File sync failed: %d\n", fresult);

        (void)f_close(&g_sLogFile);

        g_eState = STATE_FILESYSTEM_ERROR;

        return;
    }

    //
    // File fully initialized and ready for use
    //
    g_bLogFileOpened = true;

//    UARTprintf("DATALOG.CSV opened\n");

} //function ends here




/*
 * UserUSB_Write()
 * Appends data. f_write auto-advances the pointer.
 * f_sync ensures it's committed to FAT instantly.
 */
bool UserUSB_Write(const uint8_t *pData, uint32_t ui32Length)
{
    FRESULT fresult;
    UINT ui32BytesWritten;

    //
    // Check USB state
    //
    if(g_eState != STATE_DEVICE_READY)
    {
        return false;
    }

    //
    // Check file opened
    //
    if(g_bLogFileOpened == false)
    {
        return false;
    }

    //
    // Check pointer validity
    //
    if(pData == 0)
    {
        return false;
    }

    //
    // Check data length
    //
    if(ui32Length == 0)
    {
        return false;
    }

    //
    // Disabled: file pointer already positioned at EOF during
    // UserUSB_OpenLogFile(). FatFs advances fptr automatically
    // after successful writes.
    //
    /*
        //
        // Seek to end of file to append data
        //
        fresult = f_lseek(&g_sLogFile,
                          f_size(&g_sLogFile));

        if(fresult != FR_OK)
        {
            UARTprintf("Seek failed: %d\n", fresult);

            g_bLogFileOpened = false;

            g_eState = STATE_FILESYSTEM_ERROR;

            return false;
        }
    */

    //
    // Write data into file
    //
    fresult = f_write(&g_sLogFile,
                      pData,
                      ui32Length,
                      &ui32BytesWritten);

    //
    // Check write result
    //
    if(fresult != FR_OK)
    {
//        UARTprintf("File write failed: %d\n", fresult);

        (void)f_close(&g_sLogFile);

        g_bLogFileOpened = false;

        g_eState = STATE_FILESYSTEM_ERROR;

        return false;
    }

    //
    // Verify all bytes written
    //
    if(ui32BytesWritten != ui32Length)
    {
//        UARTprintf("Partial write: %u/%u bytes\n",
//                   ui32BytesWritten,
//                   ui32Length);

        (void)f_close(&g_sLogFile);

        g_bLogFileOpened = false;

        g_eState = STATE_FILESYSTEM_ERROR;

        return false;
    }

    //
    // Flush written data to USB drive
    //
    if(f_sync(&g_sLogFile) != FR_OK)
    {
//        UARTprintf("Write sync failed\n");

        (void)f_close(&g_sLogFile);

        g_bLogFileOpened = false;

        g_eState = STATE_FILESYSTEM_ERROR;

        return false;
    }

    return true;

}



/*
 * UserUSB_IsReady()
 * Returns true if drive is mounted AND log file is ready for data.
 */
bool UserUSB_IsReady(void)
{
    return ((g_eState == STATE_DEVICE_READY) &&
            (g_bLogFileOpened == true));

}


/*
 * UserUSB_CloseLogFile()
 * Clean close. Call this if you have a "Safely Remove" button.
 */
bool UserUSB_CloseLogFile(void)
{
    if(g_bLogFileOpened == false)
    {
        return true;
    }

    if(f_close(&g_sLogFile) != FR_OK)
    {
//        UARTprintf("File close failed\n");
        return false;
    }

    g_bLogFileOpened = false;

    return true;

}




//*****************************************************************************
//
// Returns current USB state.
//
// Caller can use this to distinguish between:
//   STATE_NO_DEVICE        - nothing connected
//   STATE_DEVICE_ENUM      - connected, enumerating
//   STATE_DEVICE_READY     - ready for read/write
//   STATE_UNKNOWN_DEVICE   - non-MSC device connected
//   STATE_TIMEOUT_DEVICE   - enumeration timed out
//   STATE_FILESYSTEM_ERROR - filesystem mount, open or write operation failed
//   STATE_POWER_FAULT      - power fault detected
//*****************************************************************************
tState UserUSB_GetState(void)
{
    return g_eState;

}



/////////////////////////////////////////file ends here///////////////////////////////////////////
