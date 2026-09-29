#ifndef USER_DEFINED_FILES_UD_INTERRUPTS_H_
#define USER_DEFINED_FILES_UD_INTERRUPTS_H_

#include <stdint.h>
#include <stdbool.h>

/* CAN event logger transport */
#define EVENT_LOG_CAN_ID              0x210U
#define EVENT_LOG_ACK_CAN_ID          0x211U
#define EVENT_LOG_ACK_OK              0x00U
#define EVENT_LOG_PKT_TYPE            0x0AU
#define EVENT_LOG_CAN_FRAME_SIZE      8U
#define EVENT_LOG_CAN_PAYLOAD_SIZE    6U

#define EVENT_LOG_RX_MAX_RECORD_SIZE  64U
#define EVENT_LOG_RX_MAX_FRAGMENTS    11U
#define EVENT_LOG_RX_QUEUE_DEPTH      8U

/* Timer flags */
extern volatile uint8_t cpu_time_1ms;
extern volatile uint8_t cpu_time_5ms;
extern volatile uint8_t cpu_time_10ms;
extern volatile uint8_t cpu_time_100ms;

extern volatile uint8_t tim_1ms_tick_flag;
extern volatile uint8_t tim_5ms_tick_flag;
extern volatile uint8_t tim_10ms_tick_flag;
extern volatile uint8_t tim_100ms_tick_flag;
extern volatile uint8_t tim_1s_tick_flag;

/* Diagnostics */
extern volatile uint32_t can_irq_count;
extern volatile uint32_t can_count;
extern volatile uint32_t timer_irq_count;

extern volatile uint32_t event_log_rx_frames;
extern volatile uint32_t event_log_rx_records;
extern volatile uint32_t event_log_rx_errors;

/* Timer / CAN functions */
void Timer0AIntHandler(void);
void Timer0_Init_1ms(void);
void CAN0IntHandler(void);

bool EventLogRx_PopRecord(uint8_t *buffer, uint8_t *length);
void EventLog_SendAck(void);

#endif /* USER_DEFINED_FILES_UD_INTERRUPTS_H_ */
