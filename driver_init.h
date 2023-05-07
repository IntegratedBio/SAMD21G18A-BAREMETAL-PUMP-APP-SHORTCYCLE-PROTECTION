/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef DRIVER_INIT_INCLUDED
#define DRIVER_INIT_INCLUDED

#include "atmel_start_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////
// INCLUDES AND DEFINITIONS
//////////////////////////////////////////////////////////////////////////

#include <hal_atomic.h>
#include <hal_delay.h>
#include <hal_gpio.h>
#include <hal_init.h>
#include <hal_io.h>
#include <hal_sleep.h>

#include <hal_timer.h>
#include <hpl_tc_base.h>

#include <hal_adc_sync.h>

#include <hal_usart_sync.h>
#include <stdio.h>
#include <stdio_io.h>

extern struct adc_sync_descriptor ADC_0;
extern struct usart_sync_descriptor CONSOLE_UART;
extern struct timer_descriptor TIMER_0;

//////////////////////////////////////////////////////////////////////////
//FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////

//ADC 11
void ADC_0_PORT_init(void);
void ADC_0_CLOCK_init(void);
void ADC_0_init(void);
void check_pressure_reading(void);
float average(int* values, int size);

void CONSOLE_UART_PORT_init(void);
void CONSOLE_UART_CLOCK_init(void);
void CONSOLE_UART_init(void);

void delay_driver_init(void);

// LED
void LED_0_init(void);
void LED_0_on(void);
void LED_0_off(void);
void LED_0_toggle(void);

// PUMP
void PUMP_init(void);
void PUMP_off(void);
void PUMP_on(void);

//ALARM
void ALARM_init();
void ALARM_on();
void ALARM_off();

//LOW LEVEL SWITCH
void LOW_LVL_init(void);
void LOW_LVL_status(void);

//SYSTEM INITIALIZER
void system_init(void);

#ifdef __cplusplus
}
#endif
#endif // DRIVER_INIT_INCLUDED
