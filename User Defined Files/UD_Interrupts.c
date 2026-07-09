#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <UD_Interrupts.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/ssi.h"
#include "driverlib/pin_map.h"
#include "driverlib/timer.h"
#include "driverlib/can.h"
#include "inc/hw_ints.h"
#include "inc/hw_can.h"    /* provides CAN_INT_INTID_STATUS etc. */

extern uint32_t g_sysClock;
volatile bool log_update_flag = false;

volatile uint32_t can_irq_count = 0;
volatile uint32_t can_count = 0;
volatile uint32_t timer_irq_count = 0;

extern volatile uint8_t log_frame_buf[3][6];
extern volatile uint8_t log_frame_shadow[18];
extern volatile uint8_t log_frame_buf_Read[3][6];
extern volatile uint8_t log_frames_received;
extern volatile bool log_message_ready;
extern uint8_t sRXBufLog[8];
extern tCANMsgObject sRXMsgObjLog;
extern volatile uint8_t log_frame_active;
extern volatile uint8_t log_frame_receive_mask;

/* ---------------- LOG MESSAGE DEFINITIONS ---------------- */
#define LOG_MSG_TYPE1          0x30
#define LOG_MSG_TYPE2          0x10

void Timer0AIntHandler(void)
{
    timer_irq_count++;
    /* Clear interrupt */
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    /* 5 ms tick */
    cpu_time_1ms++;
    tim_1ms_tick_flag = 1;  // 1 milli-second tick

    if(cpu_time_1ms % 5 == 0)
    {
        cpu_time_1ms = 0;
        cpu_time_5ms++;
        tim_5ms_tick_flag = 1;  // 5 milli-second tick
    }
    if(cpu_time_5ms % 2 == 0)
    {
        cpu_time_5ms = 0;
        cpu_time_10ms++;
        tim_10ms_tick_flag = 1;  // 10 milli-second tick
    }
    if(cpu_time_10ms % 10 == 0)
    {
        cpu_time_10ms = 0;
        cpu_time_100ms++;
        tim_100ms_tick_flag = 1;  // 100 milli-second tick
    }
    if(cpu_time_100ms % 10 == 0)
    {
        cpu_time_100ms = 0;
        tim_1s_tick_flag = 1;  // 1 second tick
    }
}

void Timer0_Init_1ms(void)
{
    /* Disable timer before config */
    TimerDisable(TIMER0_BASE, TIMER_A);

    /* 32-bit periodic */
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    /*
       Load value:
       timer_freq = system_clock
       period = 1 millisecond
    */
    TimerLoadSet(TIMER0_BASE, TIMER_A, (g_sysClock/1000) - 1);
}

/* ---------------- CAN0 INTERRUPT HANDLER ---------------- */

void CAN0IntHandler(void)
{

    can_irq_count++;
    uint32_t cause = CANIntStatus(CAN0_BASE, CAN_INT_STS_CAUSE);

    if (cause == CAN_INT_INTID_STATUS)
    {
        /* Clear controller status interrupt */
        (void)CANStatusGet(CAN0_BASE, CAN_STS_CONTROL);
    }
    else if (cause == 1)
    {
        can_count++;
        CANMessageGet(CAN0_BASE, 1, &sRXMsgObjLog, true);

        if (sRXBufLog[0] == LOG_MSG_TYPE1)
        {
            uint8_t seq_total = ((sRXBufLog[0] >> 4) & 0x0F) | ((sRXBufLog[1] & 0x03) << 4);
            uint8_t seq_index = ((sRXBufLog[1] >> 2) & 0x3F);
            if((!log_frame_active) || (seq_index == 0))
            {
                log_frame_active = 1;
                log_frame_receive_mask = 0;
                memset((void*) log_frame_shadow, 0x00, sizeof(log_frame_shadow));
                log_frames_received = 0;
            }

            if (seq_index < seq_total)
            {
                for (int i = 2; i < 8; i++)
                    log_frame_shadow[seq_index*6 + (i - 2)] = sRXBufLog[i];

                if(!(log_frame_receive_mask & (1U << seq_index)))
                {
                    log_frames_received++;
                    log_frame_receive_mask |= 1U << seq_index;
                }

                if(log_frames_received >= 3)
                {
                    for (int i = 0; i < 18; i++)
                    {
                        log_frame_buf[(i/6)][(i%6)] = log_frame_shadow[i];
                    }

                    log_frames_received = 0;
                    log_message_ready = true;
                }
            }
        }
        CANIntClear(CAN0_BASE, 1);
    }
}
