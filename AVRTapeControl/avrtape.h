/**************************************************************************************************************************************************************
avrtape.h

Copyright © 2024 Maksim Kryukov <fagear@mail.ru>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Created: 2021-03-16

Part of the [AVRTapeControl] project.
Main module of tape transport controller.

Main cycle, startup/power save/timing code.
Processes switches reading, user input (buttons) reading, mode indication, timing/calling tape transport mechanism state machine.

**************************************************************************************************************************************************************/

#ifndef AVRTAPE_H_
#define AVRTAPE_H_

#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "drv_cpu.h"
#include <util/delay.h>
#include "common_log.h"
#include "drv_eeprom.h"
#include "drv_io.h"
#ifdef SUPP_CRP42602Y_MECH
#include "mech_crp42602y.h"
#endif /* SUPP_CRP42602Y_MECH */
#ifdef SUPP_TANASHIN_MECH
#include "mech_tanashin.h"
#endif /* SUPP_TANASHIN_MECH */
#ifdef SUPP_KENWOOD_MECH
#include "mech_knwd.h"
#endif /* SUPP_KENWOOD_MECH */

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
#define	TASK_SCAN_PB_BTNS	(1<<6)	// Start-up scan for number of playback buttons
#define	TASK_SCAN_STEST		(1<<7)	// Start-up scan for self-test mode

uint16_t audio_centering(uint16_t in_audio, uint16_t in_center);
void ADC_read_result(void);
void audio_input_calibrate(void);

#endif /* AVRTAPE_H_ */