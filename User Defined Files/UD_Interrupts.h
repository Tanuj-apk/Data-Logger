#ifndef USER_DEFINED_FILES_UD_INTERRUPTS_H_
#define USER_DEFINED_FILES_UD_INTERRUPTS_H_

#include <stdint.h>

volatile uint8_t cpu_time_1ms;
volatile uint8_t cpu_time_5ms;
volatile uint8_t cpu_time_10ms;
volatile uint8_t cpu_time_100ms;
volatile uint8_t tim_1ms_tick_flag;
volatile uint8_t tim_5ms_tick_flag;
volatile uint8_t tim_10ms_tick_flag;
volatile uint8_t tim_100ms_tick_flag;
volatile uint8_t tim_1s_tick_flag;
volatile uint8_t log_frame_active;
volatile uint8_t log_frame_receive_mask;

void Timer0AIntHandler(void);
void Timer0_Init_1ms(void);
void CAN0IntHandler(void);

#endif /* USER_DEFINED_FILES_UD_INTERRUPTS_H_ */
