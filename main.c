/* ================= BASIC ================= */
#include <stdint.h>
#include <stdbool.h>

/* ================= DRIVERLIB ================= */
#include "inc/hw_can.h"    /* provides CAN_INT_INTID_STATUS etc. */
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/i2c.h"
#include "driverlib/can.h"
#include "driverlib/uart.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"

#include "driverlib/udma.h"
#include "usblib/usblib.h"
#include "usblib/host/usbhost.h"
#include "usblib/host/usbhmsc.h"
#include "user_usb.h"
#include "driverlib/systick.h"

/* ================= USER DEFINED ================= */
#include <EEPROM_spi.h>
#include <CH376S_uart.h>
#include <UD_Interrupts.h>
#include <EEPROM_i2c.h>
#include <EEPROM_Comm.h>

/* ================= FATFS (SD) ================= */
#include "ff.h"
#include "sd_spi.h"

#define BTN_SD          GPIO_PIN_0   // PD0 → EEPROM → SD
#define BTN_USB         GPIO_PIN_1   // PD1 → EEPROM → CH376S → USB
#define BTN_DIRECT_USB  GPIO_PIN_2   // PD2 → EEPROM → USB
#define BTN_ERASE       GPIO_PIN_3   // PD3
#define BTN_PORT        GPIO_PORTD_BASE

/* ================= DIRECT USB ================= */
#define HCD_MEMORY_SIZE 128

uint8_t g_pHCDPool[HCD_MEMORY_SIZE];
DECLARE_EVENT_DRIVER(g_sUSBEventDriver, 0, 0, USBHCDEvents);

static tUSBHostClassDriver const * const g_ppHostClassDrivers[] =
{
    &g_sUSBHostMSCClassDriver,
    &g_sUSBEventDriver
};

#define NUM_CLASS_DRIVERS (sizeof(g_ppHostClassDrivers) / sizeof(g_ppHostClassDrivers[0]))

#if defined(ccs)
#pragma DATA_ALIGN(g_sDMAControlTable, 1024)
#endif

volatile uint32_t systick_count = 0;
tDMAControlTable g_sDMAControlTable[64];
volatile uint32_t g_ui32SysTickCount = 0;

uint32_t GetTickms(void)
{
    return g_ui32SysTickCount;
}

void SysTickIntHandler(void)
{
    systick_count++;
    g_ui32SysTickCount++;
}
/* ==================================================== */
/* ================= BOOLEAN ================= */
bool sdPressed;
bool usbPressed;
bool directUSBPressed;
bool SD_BUTTON = 0;
bool USB_BUTTON = 0;
bool finish_flag = 0;
bool erasePressed = false;
volatile bool erase_flash_request = false;
volatile bool sd_dump_request = false;
volatile bool usb_dump_request = false;
volatile bool direct_usb_dump_request = false;
/* ================= NUMERICAL ================= */
uint8_t result;
uint8_t sRXBufLog[8];
uint8_t id[3];
uint16_t usb_block_index;
uint32_t val;
uint32_t g_sysClock;

/* ================= STRUCTURAL ================= */
tCANMsgObject sRXMsgObjLog;

/* ================= EXTERN ================= */
uint32_t eeprom_log_addr = FLASH_LOG_START;
volatile bool eeprom_logging_enabled = true;

/* ================= Tick Functions ================= */
void v_1sTasks(void);
void v_1msTasks(void);
void v_5msTasks(void);
void v_10msTasks(void);
void v_100msTasks(void);

/* ================= ShortForm Functions ================= */
void TIVA_Init(void);
void Buttons_Init(void);
void Dump_Functions(void);
void Tick_Functions(void);
void PeripheralEnable(void);

/* ================= Extern Functions ================= */
extern uint8_t EEPROM_to_SD(void);
extern uint8_t EEPROM_to_USB(void);
extern uint8_t EEPROM_to_USB_DIRECT(void);
int b = 0;

int main(void)
{
    TIVA_Init();
//    eeprom_log_addr = EEPROM_ReadPointer();

    if((eeprom_log_addr < FLASH_LOG_START) || (eeprom_log_addr >= FLASH_LOG_END) ||
            (((eeprom_log_addr - FLASH_LOG_START) % EVENT_LOG_FLASH_RECORD_SIZE) != 0U))
    {
        eeprom_log_addr = FLASH_LOG_START;
//        EEPROM_WritePointer(eeprom_log_addr);
    }
    while(1)
    {
        Tick_Functions();
        Dump_Functions();
//        UserUSB_Task();
    }
}

void Buttons_Init(void)
{
    GPIOPinTypeGPIOInput(BTN_PORT, BTN_SD | BTN_USB | BTN_DIRECT_USB | BTN_ERASE);
    GPIOPadConfigSet(BTN_PORT, BTN_SD | BTN_USB | BTN_DIRECT_USB | BTN_ERASE, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

void Tick_Functions(void)
{
    if(tim_1ms_tick_flag)
    {
        v_1msTasks();
        tim_1ms_tick_flag = 0;
    }

    if (tim_5ms_tick_flag)
    {
        v_5msTasks();
        tim_5ms_tick_flag = 0;
    }

    if (tim_10ms_tick_flag)
    {
        v_10msTasks();
        tim_10ms_tick_flag = 0;
    }

    if (tim_100ms_tick_flag)
    {
        v_100msTasks();
        tim_100ms_tick_flag = 0;
    }

    if (tim_1s_tick_flag)
    {
        v_1sTasks();
        tim_1s_tick_flag = 0;
    }
}

volatile uint32_t dump_count = 0;
void Dump_Functions(void)
{
    if(sd_dump_request)
    {
        sd_dump_request = false;
        eeprom_logging_enabled = false;   // STOP logging
        IntMasterDisable();
        result = EEPROM_to_SD();
        IntMasterEnable();
        eeprom_logging_enabled = true;   // resume logging
    }

    if(usb_dump_request)
    {
        usb_dump_request = false;
        eeprom_logging_enabled = false;   // STOP logging
        IntMasterDisable();
        result = EEPROM_to_USB();
        IntMasterEnable();
        eeprom_logging_enabled = true;   // resume logging
    }

    if(direct_usb_dump_request)
    {
        direct_usb_dump_request = false;
        eeprom_logging_enabled = false;
        IntDisable(INT_CAN0);
        IntDisable(INT_TIMER0A);
        SysTickIntDisable();
        result = EEPROM_to_USB_DIRECT();
        SysTickIntEnable();
        IntEnable(INT_TIMER0A);
        IntEnable(INT_CAN0);
        eeprom_logging_enabled = true;
    }

    if(erase_flash_request)
    {
        dump_count++;
        erase_flash_request = false;
        eeprom_logging_enabled = false;
        IntMasterDisable();
        Flash_SectorErase(0x000000);
        eeprom_log_addr = FLASH_LOG_START;
        EEPROM_WritePointer(eeprom_log_addr);
        IntMasterEnable();
        eeprom_logging_enabled = true;
    }
}

void PeripheralEnable(void)
{
    /* Enable GPIOA */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));

    /* Enable GPIOB */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB));

    /* Enable GPIOD */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD));

    /* Enable GPIOE */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE));

    /* Enable GPIOC */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOC));

    /* Enable UART7 */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART7);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART7));

    /* Enable CAN0 */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_CAN0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_CAN0));

    /* Enable Timer0 */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));

    /* Enable SSI0 */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_SSI0));

    /* Enable SSI1 */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI1);
    SysCtlPeripheralReset(SYSCTL_PERIPH_SSI1);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_SSI1));

    /* Enable uDMA */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UDMA));

    /* Enable GPIOL */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOL));

    /* Enable GPIOQ */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOQ);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOQ));
}

void TIVA_Init(void)
{
    finish_flag = 0;
    g_sysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);
    SysTickPeriodSet(g_sysClock / 1000);   // 1 ms
    SysTickIntEnable();
    SysTickEnable();
    PeripheralEnable();

    /* USB D- D+ */
    GPIOPinTypeUSBAnalog(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    /* USB power switch enable */
    GPIOPinConfigure(GPIO_PD6_USB0EPEN);
    GPIOPinTypeUSBDigital(GPIO_PORTD_BASE, GPIO_PIN_6);

    /* USB ID + VBUS sense */
    GPIOPinTypeUSBAnalog(GPIO_PORTL_BASE, GPIO_PIN_6 | GPIO_PIN_7);

    /* USB power fault */
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_4);

    /* PC4 = U7RX, PC5 = U7TX */
    GPIOPinConfigure(GPIO_PC4_U7RX);
    GPIOPinConfigure(GPIO_PC5_U7TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    /* PA0 = CAN0RX, PA1 = CAN0TX */
    GPIOPinConfigure(GPIO_PA0_CAN0RX);
    GPIOPinConfigure(GPIO_PA1_CAN0TX);
    GPIOPinTypeCAN(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART7_BASE, g_sysClock, 9600, UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);

    SPI_Init();

//    /** Verify that the external SPI NOR flash is actually responding.
//     * Flash_Probe() is bounded and will not wait forever if the flash is missing or disconnected. */
//    if(!Flash_Probe())
//    {
//        /** Keep the TIVA running, but disable event logging because
//         * the log memory is unavailable. */
//        eeprom_logging_enabled = false;
//    }

    I2C1_Init();
    Buttons_Init();
    EEPROM_SPI_Init();
    Timer0_Init_1ms();
    CANInit(CAN0_BASE);
    CANBitRateSet(CAN0_BASE, g_sysClock, 500000);

    /* Configure RX message object */
    sRXMsgObjLog.ui32MsgID       = EVENT_LOG_CAN_ID;
    sRXMsgObjLog.ui32MsgIDMask   = 0;
    sRXMsgObjLog.ui32Flags       = MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_EXTENDED_ID;
    sRXMsgObjLog.ui32MsgLen      = 8;
    sRXMsgObjLog.pui8MsgData     = sRXBufLog;

    CANMessageSet(CAN0_BASE, 1, &sRXMsgObjLog, MSG_OBJ_TYPE_RX);

    /* Enable CAN0 Interrupt */
    CANIntEnable(CAN0_BASE, CAN_INT_MASTER | CAN_INT_ERROR | CAN_INT_STATUS);
    /* Enable interrupt */
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    IntEnable(INT_CAN0);

    /* Enable NVIC interrupt */
    IntEnable(INT_TIMER0A);

    IntMasterEnable();

    CANEnable(CAN0_BASE);
    /* Start timer */
    TimerEnable(TIMER0_BASE, TIMER_A);

    uint32_t ui32PLLRate;
    uDMAEnable();
    uDMAControlBaseSet(g_sDMAControlTable);
    USBStackModeSet(0, eUSBModeHost, 0);
    USBHCDRegisterDrivers(0, g_ppHostClassDrivers, NUM_CLASS_DRIVERS);
    UserUSB_Init();
    USBHCDPowerConfigInit(0, USBHCD_VBUS_AUTO_HIGH | USBHCD_VBUS_FILTER);
    SysCtlVCOGet(SYSCTL_XTAL_25MHZ, &ui32PLLRate);
    USBHCDFeatureSet(0, USBLIB_FEATURE_CPUCLK, &g_sysClock);
    USBHCDFeatureSet(0, USBLIB_FEATURE_USBPLL, &ui32PLLRate);
    USBHCDInit(0, g_pHCDPool, HCD_MEMORY_SIZE);
}

void v_1msTasks(void)
{
    val = GPIOPinRead(BTN_PORT, BTN_SD | BTN_USB | BTN_DIRECT_USB | BTN_ERASE);
    sdPressed        = ((val & BTN_SD) == 0);
    usbPressed       = ((val & BTN_USB) == 0);
    directUSBPressed = ((val & BTN_DIRECT_USB) == 0);
    erasePressed     = ((val & BTN_ERASE) == 0);
}

void v_5msTasks(void)
{
    uint8_t wire_record[EVENT_LOG_RX_MAX_RECORD_SIZE];
    uint8_t wire_length;
    uint8_t flash_record[EVENT_LOG_FLASH_RECORD_SIZE];

    if(eeprom_logging_enabled)
    {
        if(EventLogRx_PopRecord(wire_record, &wire_length))
        {
            if(EventLog_CompressRecord(wire_record, wire_length, flash_record))
            {
                /* Acknowledge the CPU only after the event is safely stored. */
                EventLog_SendAck();

                if(Flash_AppendEventRecord(flash_record))
                {
                    /* Persist the flash write pointer only after the complete 16-byte record has been programmed. */
                    EEPROM_WritePointer(eeprom_log_addr);
                }
                else
                {
                    eeprom_logging_enabled = false;
                }
            }
            else
            {
                /** Invalid or unsupported CPU event record. */
                event_log_rx_errors++;
            }
        }
    }

//    uint8_t wire_record[EVENT_LOG_RX_MAX_RECORD_SIZE];
//    uint8_t wire_length;
//    uint8_t flash_record[EVENT_LOG_FLASH_RECORD_SIZE];
//
//    if(eeprom_logging_enabled)
//    {
//        if(EventLogRx_PopRecord(wire_record, &wire_length))
//        {
//            /* Reassembled packet successfully parsed/validated. */
//            if(EventLog_CompressRecord(wire_record, wire_length, flash_record))
//            {
//                /** ACK is intentionally BEFORE the flash write.
//                 * At this point the complete packet has already
//                 * been received and parsed successfully. */
//                EventLog_SendAck();
//
//                /* Clear any previous runtime flash fault. */
//                EEPROM_SPI_ClearFault();
//
//                /* Write the parsed 16-byte record to SPI NOR. */
//                if(Flash_AppendEventRecord(flash_record) && !EEPROM_SPI_HasFault())
//                {
//                    /* Persist the pointer only after the flash
//                     * operation completed without a SPI fault. */
//                    EEPROM_WritePointer(eeprom_log_addr);
//                }
//                else
//                {
//                    /** Flash communication/programming failed.
//                     * Do not continue attempting to log events.
//                     * Most importantly, the CPU never gets trapped
//                     * in an infinite SPI polling loop. */
//                    eeprom_logging_enabled = false;
//                }
//            }
//            else
//            {
//                /* Invalid or unsupported CPU event record. */
//                event_log_rx_errors++;
//            }
//        }
//    }
}

void v_10msTasks(void)
{

}

void v_100msTasks(void)
{
    val = 0;
    static bool lastSDPressed = false;
    static bool lastUSBPressed = false;
    static bool lastDirectUSBPressed = false;
    static bool lastErasePressed = false;

    /* SD Dump */
    if(sdPressed && !lastSDPressed)
    {
        sd_dump_request = true;
    }

    /* CH376 USB Dump */
    if(usbPressed && !lastUSBPressed)
    {
        usb_dump_request = true;
    }

    /* Native USB Dump */
    if(directUSBPressed && !lastDirectUSBPressed)
    {
        direct_usb_dump_request = true;
    }

    /* Erase Flash */
    if(erasePressed && !lastErasePressed)
    {
        erase_flash_request = true;
    }

    lastSDPressed        = sdPressed;
    lastUSBPressed       = usbPressed;
    lastDirectUSBPressed = directUSBPressed;
    lastErasePressed     = erasePressed;
}

void v_1sTasks(void)
{

}
