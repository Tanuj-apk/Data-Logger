#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "UD_Interrupts.h"

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_can.h"

#include "driverlib/timer.h"
#include "driverlib/can.h"

/* External application objects */
extern uint32_t g_sysClock;
extern uint8_t sRXBufLog[8];
extern tCANMsgObject sRXMsgObjLog;

/* Timer variables */
volatile uint8_t cpu_time_1ms   = 0U;
volatile uint8_t cpu_time_5ms   = 0U;
volatile uint8_t cpu_time_10ms  = 0U;
volatile uint8_t cpu_time_100ms = 0U;

volatile uint8_t tim_1ms_tick_flag   = 0U;
volatile uint8_t tim_5ms_tick_flag   = 0U;
volatile uint8_t tim_10ms_tick_flag  = 0U;
volatile uint8_t tim_100ms_tick_flag = 0U;
volatile uint8_t tim_1s_tick_flag    = 0U;

/* Diagnostics */
volatile uint32_t can_irq_count = 0U;
volatile uint32_t can_count = 0U;
volatile uint32_t timer_irq_count = 0U;

volatile uint32_t event_log_rx_frames  = 0U;
volatile uint32_t event_log_rx_records = 0U;
volatile uint32_t event_log_rx_errors  = 0U;

/* RX reassembly */
static volatile uint8_t event_log_rx_shadow[EVENT_LOG_RX_MAX_RECORD_SIZE];
static volatile uint8_t event_log_rx_queue[EVENT_LOG_RX_QUEUE_DEPTH][EVENT_LOG_RX_MAX_RECORD_SIZE];
static volatile uint8_t event_log_rx_queue_len[EVENT_LOG_RX_QUEUE_DEPTH];

static volatile uint8_t event_log_rx_q_head = 0U;
static volatile uint8_t event_log_rx_q_tail = 0U;

static volatile bool event_log_rx_active = false;
static volatile uint8_t event_log_rx_seq_total = 0U;
static volatile uint8_t event_log_rx_frames_received = 0U;
static volatile uint16_t event_log_rx_receive_mask = 0U;

/* Timer ISR */
void Timer0AIntHandler(void)
{
    timer_irq_count++;
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    cpu_time_1ms++;
    tim_1ms_tick_flag = 1U;

    if((cpu_time_1ms % 5U) == 0U)
    {
        cpu_time_1ms = 0U;
        cpu_time_5ms++;
        tim_5ms_tick_flag = 1U;
    }

    if((cpu_time_5ms % 2U) == 0U)
    {
        cpu_time_5ms = 0U;
        cpu_time_10ms++;
        tim_10ms_tick_flag = 1U;
    }

    if((cpu_time_10ms % 10U) == 0U)
    {
        cpu_time_10ms = 0U;
        cpu_time_100ms++;
        tim_100ms_tick_flag = 1U;
    }

    if((cpu_time_100ms % 10U) == 0U)
    {
        cpu_time_100ms = 0U;
        tim_1s_tick_flag = 1U;
    }
}

/* Timer initialization */
void Timer0_Init_1ms(void)
{
    TimerDisable(TIMER0_BASE, TIMER_A);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (g_sysClock / 1000U) - 1U);
}

/* CAN event logger receiver */
void CAN0IntHandler(void)
{
    uint32_t cause;
    can_irq_count++;
    cause = CANIntStatus(CAN0_BASE, CAN_INT_STS_CAUSE);

    if(cause == CAN_INT_INTID_STATUS)
    {
        (void)CANStatusGet(CAN0_BASE, CAN_STS_CONTROL);
        return;
    }

    if(cause == 1U)
    {
        uint8_t pkt_type;
        uint8_t seq_total;
        uint8_t seq_index;
        uint16_t offset;
        uint8_t i;

        can_count++;
        CANMessageGet(CAN0_BASE, 1U, &sRXMsgObjLog, true);
        pkt_type = (uint8_t)(sRXBufLog[0] & 0x0FU);
        if(pkt_type != EVENT_LOG_PKT_TYPE)
        {
            CANIntClear(CAN0_BASE, 1U);
            return;
        }

        seq_total = (uint8_t)((((sRXBufLog[0]) >> 4U) & 0x0FU) | ((sRXBufLog[1] & 0x03U) << 4U));
        seq_index = (uint8_t)((sRXBufLog[1] >> 2U) & 0x3FU);

        if((seq_total == 0U) || (seq_total > EVENT_LOG_RX_MAX_FRAGMENTS) || (seq_index >= seq_total))
        {
            event_log_rx_errors++;
            CANIntClear(CAN0_BASE, 1U);
            return;
        }

        if(!event_log_rx_active)
        {
            if(seq_index != 0U)
            {
                CANIntClear(CAN0_BASE, 1U);
                return;
            }

            event_log_rx_active = true;
            event_log_rx_seq_total = seq_total;
            event_log_rx_receive_mask = 0U;
            event_log_rx_frames_received = 0U;

            memset((void *)event_log_rx_shadow, 0, sizeof(event_log_rx_shadow));
        }

        /** A new fragment-0 while a record is still incomplete means the previous record cannot be completed. Drop it and restart. */
        if((seq_index == 0U) && (event_log_rx_frames_received != 0U))
        {
            event_log_rx_errors++;
            event_log_rx_seq_total = seq_total;
            event_log_rx_receive_mask = 0U;
            event_log_rx_frames_received = 0U;

            memset((void *)event_log_rx_shadow, 0, sizeof(event_log_rx_shadow));
        }

        if(seq_total != event_log_rx_seq_total)
        {
            event_log_rx_errors++;
            event_log_rx_active = false;
            event_log_rx_receive_mask = 0U;
            event_log_rx_frames_received = 0U;
            CANIntClear(CAN0_BASE, 1U);
            return;
        }

        offset = (uint16_t)(seq_index * EVENT_LOG_CAN_PAYLOAD_SIZE);
        for(i = 0U; i < EVENT_LOG_CAN_PAYLOAD_SIZE; i++)
        {
            if((offset + i) < EVENT_LOG_RX_MAX_RECORD_SIZE)
            {
                event_log_rx_shadow[offset + i] = sRXBufLog[2U + i];
            }
        }

        if((event_log_rx_receive_mask & (uint16_t)(1U << seq_index)) == 0U)
        {
            event_log_rx_receive_mask |= (uint16_t)(1U << seq_index);
            event_log_rx_frames_received++;
            event_log_rx_frames++;
        }

        if(event_log_rx_frames_received >= event_log_rx_seq_total)
        {
            uint8_t payload_length;
            uint16_t wire_length;
            uint8_t next_head;

            /** CPU wire byte 17 contains payload length.
             * Total logical record = 18 + payload length. */
            payload_length = event_log_rx_shadow[17U];
            wire_length = (uint16_t)(18U + payload_length);
            if((wire_length <= ((uint16_t)event_log_rx_seq_total * EVENT_LOG_CAN_PAYLOAD_SIZE)) && (wire_length <= EVENT_LOG_RX_MAX_RECORD_SIZE))
            {
                next_head = (uint8_t)((event_log_rx_q_head + 1U) % EVENT_LOG_RX_QUEUE_DEPTH);
                if(next_head != event_log_rx_q_tail)
                {
                    memcpy((void *)event_log_rx_queue[event_log_rx_q_head], (const void *)event_log_rx_shadow, wire_length);
                    event_log_rx_queue_len[event_log_rx_q_head] = (uint8_t)wire_length;
                    event_log_rx_q_head = next_head;
                    event_log_rx_records++;
                }
                else
                {
                    event_log_rx_errors++;
                }
            }
            else
            {
                event_log_rx_errors++;
            }

            event_log_rx_active = false;
            event_log_rx_seq_total = 0U;
            event_log_rx_receive_mask = 0U;
            event_log_rx_frames_received = 0U;
        }
        CANIntClear(CAN0_BASE, 1U);
    }
}

/* ============================================================
 * Send ACK to the CPU after a complete event record has been
 * successfully stored in SPI NOR flash.
 *
 * CAN ID : 0x211 (extended)
 *
 * Byte 0..1 : CPU transmission CAN ID = 0x0210
 * Byte 2    : packet type = 0x0A
 * Byte 3    : status = 0x00 (ACK OK)
 * Byte 4..7 : reserved
 * ============================================================ */
void EventLog_SendAck(void)
{
    tCANMsgObject txMsg;
    uint8_t txData[8];

    txData[0] = (uint8_t)((EVENT_LOG_CAN_ID >> 8U) & 0xFFU);
    txData[1] = (uint8_t)(EVENT_LOG_CAN_ID & 0xFFU);
    txData[2] = EVENT_LOG_PKT_TYPE;
    txData[3] = EVENT_LOG_ACK_OK;
    txData[4] = 0x00U;
    txData[5] = 0x00U;
    txData[6] = 0x00U;
    txData[7] = 0x00U;

    txMsg.ui32MsgID = EVENT_LOG_ACK_CAN_ID;
    txMsg.ui32MsgIDMask = 0U;
    txMsg.ui32Flags = MSG_OBJ_EXTENDED_ID;
    txMsg.ui32MsgLen = 8U;
    txMsg.pui8MsgData = txData;

    CANMessageSet(CAN0_BASE, 2U, &txMsg, MSG_OBJ_TYPE_TX);
}

/* Pop a complete record from the RX queue. */
bool EventLogRx_PopRecord(uint8_t *buffer, uint8_t *length)
{
    uint8_t tail;
    uint8_t len;

    if((buffer == NULL) || (length == NULL))
    {
        return false;
    }

    if(event_log_rx_q_tail == event_log_rx_q_head)
    {
        return false;
    }

    tail = event_log_rx_q_tail;
    len = event_log_rx_queue_len[tail];

    memcpy(buffer, (const void *)event_log_rx_queue[tail], len);
    *length = len;
    event_log_rx_q_tail = (uint8_t)((event_log_rx_q_tail + 1U) % EVENT_LOG_RX_QUEUE_DEPTH);
    return true;
}
