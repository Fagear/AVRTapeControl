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
#ifdef SUPP_TANASHIN_MECH
#include "mech_tanashin.h"
#endif /* SUPP_TANASHIN_MECH */
#ifdef SUPP_CRP42602Y_MECH
#include "mech_crp42602y.h"
#endif /* SUPP_CRP42602Y_MECH */
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

// Flags for [kbd_state], [kbd_pressed] and [kbd_released].
#define USR_BTN_REWIND		(1<<0)	// Rewind button
#define USR_BTN_PLAY_REV	(1<<1)	// Play in reverse button
#define USR_BTN_STOP		(1<<2)	// Stop button
#define USR_BTN_RECORD		(1<<3)	// Record button
#define USR_BTN_PLAY		(1<<4)	// Play/reverse direction button
#define USR_BTN_FFORWARD	(1<<5)	// Fast forward button

// Indicators on the SPI 595 extender at [SPI_IDX_IND].
#define IND_ERROR			(1<<0)	// Transport error indicator
#define IND_TAPE			(1<<1)	// Tape presence indicator (cassette illumination?)
#define IND_STOP			(1<<2)	// Stop indicator
#define IND_REC				(1<<3)	// Record indicator
#define IND_REWIND			(1<<4)	// Rewind indicator
#define IND_PLAY_REV		(1<<5)	// Play in reverse direction indicator
#define IND_PLAY_DIR		(1<<5)	// Playback direction indicator
#define IND_PLAY			(1<<6)	// Playback indicator
#define IND_PLAY_FWD		(1<<6)  // Play in forward direction indicator
#define IND_FFORWARD		(1<<7)	// Fast forward indicator

// Supported tape transports.
enum
{
	TTR_TYPE_CRP42602Y,				// CRP42602Y mechanism from AliExpress
	TTR_TYPE_KENWOOD,				// Kenwood mechanism
	TTR_TYPE_TANASHIN,				// Tanashin TN-21ZLG clone mechanism from AliExpress
};

// Index of byte in SPI bus extenders for [u8a_spi_buf].
enum
{
	SPI_IDX_IND,					// Regular transport mode indicators
	SPI_IDX_MAX						// Index limit
};

// EEPROM settings offsets.
enum
{
	EPS_MARKER,						// Start marker position
	EPS_TTR_TYPE,					// Transport type (if several types are enabled on compile time)
	EPS_TTR_FTRS,					// Transport features (tacho in stop, reverse enable, etc.)
	EPS_SRV_FTRS,					// Service features (auto-reverse, auto-rewind, etc.)
};

#define SLEEP_INHIBIT_2HZ	6		// Time for sleep inhibition with 2HZ rate

void scan_pb_buttons(void);
void scan_selftest_buttons(void);
void process_user(void);
void UART_dump_settings(uint8_t in_ttr_settings, uint8_t in_srv_settings);
int main(void);

#endif /* AVRTAPE_H_ */