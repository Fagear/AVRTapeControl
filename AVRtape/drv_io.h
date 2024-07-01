/**************************************************************************************************************************************************************
drv_io.h

Copyright © 2023 Maksim Kryukov <fagear@mail.ru>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Created: 2021-04-04

Hardware defines (pseudo-HAL) and setup routines.

**************************************************************************************************************************************************************/

#ifndef DRV_IO_H_
#define DRV_IO_H_

#include <avr/io.h>
#include "drv_spi.h"

#ifdef UART_TERM
#include "drv_uart.h"
#endif /* UART_TERM */

// User-input buttons.
#define BTN_PORT_1			PORTC
#define BTN_DIR_1			DDRC
#define BTN_SRC_1			PINC
#define BTN_1				(1<<5)
#define BTN_2				(1<<4)
#define BTN_3				(1<<3)
#define BTN_4				(1<<2)
#define BTN_5				(1<<1)
#define BTN_6				(1<<0)
#define BTN_REWD			BTN_1						// Rewind button
#define BTN_PLAY_REV		BTN_2						// Play in reverse button
#define BTN_STOP			BTN_3						// Stop button
#define BTN_REC				BTN_4						// Record button
#define BTN_PLAY			BTN_5						// Play/reverse button
#define BTN_FFWD			BTN_6						// Fast forward button
#define BTN_REWD_STATE		(BTN_SRC_1&BTN_REWD)
#define BTN_PLAY_REV_STATE	(BTN_SRC_1&BTN_PLAY_REV)
#define BTN_STOP_STATE		(BTN_SRC_1&BTN_STOP)
#define BTN_REC_STATE		(BTN_SRC_1&BTN_REC)
#define BTN_PLAY_STATE		(BTN_SRC_1&BTN_PLAY)
#define BTN_FFWD_STATE		(BTN_SRC_1&BTN_FFWD)
#define BTN_EN_INTR1		PCMSK1|=(1<<PCINT8)|(1<<PCINT9)|(1<<PCINT10)|(1<<PCINT11)|(1<<PCINT12)|(1<<PCINT13)
#define BTN_EN_INTR2		PCICR|=(1<<PCIE1)
#define BTN_DIS_INTR2		PCICR&=~(1<<PCIE1)
#define BTN_INT				PCINT1_vect					// Pin change interrupt

// Sensors and switches.
#define SW_PORT				PORTD
#define SW_DIR				DDRD
#define SW_SRC				PIND
#define SW_1				(1<<2)
#define SW_2				(1<<3)
#define SW_3				(1<<4)
#define SW_4				(1<<5)
#define SW_5				(1<<6)
#define SW_TACH				SW_1						// Tape movement tachometer
#define SW_STOP				SW_2						// Tape transport in mechanical "STOP" mode/"HOME" state
#define SW_TAPE_IN			SW_3						// Tape presence sensor
#define SW_NOREC_FWD		SW_4						// Record inhibit in forward direction
#define SW_NOREC_REV		SW_5						// Record inhibit in reverse direction
#define SW_TACHO_STATE		(SW_SRC&SW_TACH)
#define SW_STOP_STATE		(SW_SRC&SW_STOP)
#define SW_TAPE_IN_STATE	(SW_SRC&SW_TAPE_IN)
#define SW_NOREC_FWD_STATE	(SW_SRC&SW_NOREC_FWD)
#define SW_NOREC_REV_STATE	(SW_SRC&SW_NOREC_REV)
#define SW_EN_INTR1			PCMSK2|=(1<<PCINT19)|(1<<PCINT20)|(1<<PCINT21)|(1<<PCINT22)
#define SW_EN_INTR2			PCICR|=(1<<PCIE2)
#define SW_DIS_INTR2		PCICR&=~(1<<PCIE2)
#define SW_INT				PCINT2_vect					// Pin change interrupt

// Playback mute output control.
#define MUTE_EN_PORT		PORTD
#define MUTE_EN_DIR			DDRD
#define MUTE_EN_SRC			PIND
#define MUTE_EN_BIT			(1<<0)
#define MUTE_EN_ON			MUTE_EN_PORT|=MUTE_EN_BIT	// Enable mute for playback amplifier in non-playback state
#define MUTE_EN_OFF			MUTE_EN_PORT&=~MUTE_EN_BIT	// Disable mute for playback amplifier in non-playback state
#define MUTE_EN_STATE		(MUTE_EN_SRC&MUTE_EN_BIT)

// Record output control.
#define REC_EN_PORT			PORTD
#define REC_EN_DIR			DDRD
#define REC_EN_SRC			PIND
#define REC_EN_BIT			(1<<7)
#define REC_EN_ON			REC_EN_PORT|=REC_EN_BIT		// Record enable for bias circuit and head amplifier mode
#define REC_EN_OFF			REC_EN_PORT&=~REC_EN_BIT	// Record disable for bias circuit and head amplifier mode
#define REC_EN_STATE		(REC_EN_SRC&REC_EN_BIT)

// Transport mode actuator/solenoid control.
#define SOL_PORT			PORTB
#define SOL_DIR				DDRB
#define SOL_SRC				PINB
#define SOL_BIT				(1<<0)
#define SOLENOID_ON			SOL_PORT|=SOL_BIT			// Energize transport actuator
#define SOLENOID_OFF		SOL_PORT&=~SOL_BIT			// Turn off transport actuator
#define SOLENOID_STATE		(SOL_SRC&SOL_BIT)

// Capstan motor control.
#define CAPSTAN_PORT		PORTB
#define CAPSTAN_DIR			DDRB
#define CAPSTAN_SRC			PINB
#define CAPSTAN_BIT			(1<<1)
#define CAPSTAN_ON			CAPSTAN_PORT|=CAPSTAN_BIT	// Capstan motor enable
#define CAPSTAN_OFF			CAPSTAN_PORT&=~CAPSTAN_BIT	// Capstan motor disable
#define CAPSTAN_STATE		(CAPSTAN_SRC&CAPSTAN_BIT)

// Service motor control (alternative to capstan/solenoid control).
#define SMTR_PORT			PORTB
#define SMTR_DIR			DDRB
#define SMTR_SRC			PINB
#define SMTR_BIT1			(1<<0)
#define SMTR_BIT2			(1<<1)
#define SMTR_DIR1_ON		SMTR_PORT|=SMTR_BIT1
#define SMTR_DIR1_OFF		SMTR_PORT&=~SMTR_BIT1
#define SMTR_DIR2_ON		SMTR_PORT|=SMTR_BIT2
#define SMTR_DIR2_OFF		SMTR_PORT&=~SMTR_BIT2

// Watchdog setup.
#define WDT_RESET_DIS		MCUSR&=~(1<<WDRF)
#define WDT_PREP_OFF		WDTCSR|=(1<<WDCE)|(1<<WDE)
#define WDT_SW_OFF			WDTCSR=0x00
#define WDT_FLUSH_REASON	MCUSR=(0<<WDRF)|(0<<BORF)|(0<<EXTRF)|(0<<PORF)
#define WDT_PREP_ON			WDTCSR|=(1<<WDCE)|(1<<WDE)
#define WDT_SW_ON			WDTCSR=(1<<WDE)|(1<<WDP0)|(1<<WDP1)|(1<<WDP2)	// MCU reset after ~2.0 s

// System timer setup.
#define SYST_INT			TIMER2_COMPA_vect			// Interrupt vector alias
#define SYST_CONFIG1		TCCR2A=(1<<WGM21)			// CTC mode (clear on compare with OCR)
#define SYST_CONFIG2		OCR2A=124					// Cycle clock: INclk/(1+124), 1000 Hz cycle
#define SYST_EN_INTR		TIMSK2|=(1<<OCIE2A)			// Enable interrupt
#define SYST_DIS_INTR		TIMSK2&=~(1<<OCIE2A)		// Disable interrupt
#define SYST_START			TCCR2B|=(1<<CS21)			// Start timer with clk/8 clock (125 kHz)
#define SYST_STOP			TCCR2B&=~((1<<CS20)|(1<<CS21)|(1<<CS22))	// Stop timer
#define SYST_DATA_8			TCNT2						// Count register
#define SYST_RESET			SYST_DATA_8=0				// Reset count

// Power consumption optimizations.
#define PWR_COMP_OFF		ACSR|=(1<<ACD)
#define PWR_ADC_OFF			PRR|=(1<<PRADC)
#define PWR_T0_OFF			PRR|=(1<<PRTIM0)
#define PWR_T1_OFF			PRR|=(1<<PRTIM1)
#define PWR_T2_OFF			PRR|=(1<<PRTIM2)
#define PWR_I2C_OFF			PRR|=(1<<PRTWI)
#define PWR_SPI_OFF			PRR|=(1<<PRSPI)
#define PWR_UART_OFF		PRR|=(1<<PRUSART0)

//-------------------------------------- IO initialization.
inline void HW_init(void)
{
	// Init power output control.
	SMTR_PORT&=~(SMTR_BIT1|SMTR_BIT2);		// Disable pull-ups/set output to "0"
	SMTR_DIR|=(SMTR_BIT1|SMTR_BIT2);		// Set pins as outputs
	SOL_PORT&=~SOL_BIT;						// Disable pull-ups/set output to "0"
	SOL_DIR|=SOL_BIT;						// Set pin as output
	CAPSTAN_PORT&=~CAPSTAN_BIT;				// Disable pull-ups/set output to "0"
	CAPSTAN_DIR|=CAPSTAN_BIT;				// Set pin as output

	// Init playback mute control.
	MUTE_EN_ON;								// Set output to "1"
	MUTE_EN_DIR|=MUTE_EN_BIT;				// Set pin as output

	// Init record control.
	REC_EN_OFF;								// Disable pull-ups/set output to "0"
	REC_EN_DIR|=REC_EN_BIT;					// Set pin as output

	// Init user buttons inputs.
	BTN_DIR_1&=~(BTN_1|BTN_2|BTN_3|BTN_4|BTN_5|BTN_6);	// Set pins as inputs
	BTN_PORT_1|=BTN_1|BTN_2|BTN_3|BTN_4|BTN_5|BTN_6;	// Turn on pull-ups
	// Init transport switches inputs.
	SW_DIR&=~(SW_1|SW_2|SW_3|SW_4|SW_5);	// Set pins as inputs
	SW_PORT|=SW_1|SW_2|SW_3|SW_4|SW_5;		// Turn on pull-ups
	// Pre-configure (but not enable) pin change interrupts for sleep mode.
	BTN_EN_INTR1;
	SW_EN_INTR1;

	// System timing.
	SYST_CONFIG1;
	SYST_CONFIG2;
	SYST_RESET;
	SYST_EN_INTR;

	// Init SPI interface.
	SPI_init_master();
#ifdef UART_TERM
	// Init USART interface.
	UART_set_speed(UART_SPEED);
	UART_enable();
#endif /* UART_TERM */

	// Turn off unused modules for power saving.
	PWR_COMP_OFF; PWR_ADC_OFF; PWR_I2C_OFF; PWR_T0_OFF; PWR_T1_OFF;
#ifndef UART_TERM
	PWR_UART_OFF;
#endif /* UART_TERM */
}

#endif /* DRV_IO_H_ */