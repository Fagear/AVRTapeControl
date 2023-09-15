/*
 * avrtape.h
 *
 * Created:			2021-04-13 14:16:02
 * Modified:		2023-09-07
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		Main header, defines for flags, enums and constants for core logic.
 *
 */

#ifndef AVRTAPE_H_
#define AVRTAPE_H_

#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "common_log.h"
#include "drv_cpu.h"
#include "drv_eeprom.h"
#include "drv_io.h"
#include "mech_crp42602y.h"
#include "mech_tanashin.h"

// Flags for [u8i_interrupts] and [u8_buf_interrupts].
#define INTR_SYS_TICK		(1<<0)
#define INTR_READ_ADC		(1<<1)
#define INTR_SPI_READY		(1<<2)
#define INTR_UART_SENT		(1<<3)
#define INTR_UART_RECEIVED	(1<<4)

// Flags for [u8_tasks].
#define	TASK_500HZ			(1<<0)	// 500 Hz event
#define	TASK_50HZ			(1<<1)	// 50 Hz event
#define	TASK_10HZ			(1<<2)	// 10 Hz event
#define	TASK_2HZ			(1<<3)	// 2 Hz event
#define	TASK_SLOW_BLINK		(1<<4)	// Indicator slow blink source
#define	TASK_FAST_BLINK		(1<<5)	// Indicator fast blink source

uint16_t audio_centering(uint16_t in_audio, uint16_t in_center);
void ADC_read_result(void);
void audio_input_calibrate(void);

#endif /* AVRTAPE_H_ */