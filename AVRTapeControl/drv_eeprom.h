/**************************************************************************************************************************************************************
drv_eeprom.h

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

Created: 2010-09-07

EEPROM driver for AVR MCUs and AtmelStudio/AVRStudio/WinAVR/avr-gcc compilers.
The driver allows single-byte operations with EEPROM (erase/write/read).
The driver also allows to read and write segments of data.
This driver automatically adapts to different data target sizes and EEPROM size of the MCU (you have to set [EEPROM_TARGET_SIZE]).
This driver handles automatic boot-up data search and load via first [EEPROM_START_MARKER] byte in data.
This driver can detect corruption in the data with help of CRC-8 algorithm (at the end of data, CRC calculation functions are in a separate file).
This driver implements several wear-leveling techniques: moving active segment around all available memory,
inverting bytes (zeros are more often in data than 0xFF), data-dependent writes and erases.
There are two variants of interrupt-sensitive functions: ones with "_intfree" must be used after you turn off all interrupts,
other functions will deal with interrupts themselves.
It is NOT RECOMMENDED to use any of the functions of this driver in an interrupt routines.

Part of the [AVRTapeControl] project.
Supported MCUs:	ATmega32(A), ATmega88(A/PA), ATmega168(A/PA), ATmega328(P).

**************************************************************************************************************************************************************/

#ifndef DRV_EEPROM_H_
#define DRV_EEPROM_H_

#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "config.h"		// Contains [EEPROM_TARGET_SIZE]
#include "calc_CRC.h"	// Contains CRC-8 calculation routines

// AVR EEPROM size (bytes)
#define EEPROM_ROM_SIZE		(E2END+1)

// Total size of data to be stored in EEPROM.
#ifndef EEPROM_TARGET_SIZE
	#error You must set EEPROM target size! (EEPROM_TARGET_SIZE)
#elif EEPROM_TARGET_SIZE>1023
	#define EEPROM_STORE_SIZE	2048
	#define EEP_16BIT_ADDR		1
#elif EEPROM_TARGET_SIZE>511
	#define EEPROM_STORE_SIZE	1024
	#define EEP_16BIT_ADDR		1
#elif EEPROM_TARGET_SIZE>255
	#define EEPROM_STORE_SIZE	512
	#define EEP_16BIT_ADDR		1
#elif EEPROM_TARGET_SIZE>127
	#define EEPROM_STORE_SIZE	256
	#define EEP_16BIT_ADDR		1
#elif EEPROM_TARGET_SIZE>63
	#define EEPROM_STORE_SIZE 	128
	#undef EEP_16BIT_ADDR
#elif EEPROM_TARGET_SIZE>31
	#define EEPROM_STORE_SIZE	64
	#undef EEP_16BIT_ADDR
#elif EEPROM_TARGET_SIZE>15
	#define EEPROM_STORE_SIZE	32
	#undef EEP_16BIT_ADDR
#else
	#define EEPROM_STORE_SIZE	16
	#undef EEP_16BIT_ADDR
#endif
#if EEPROM_STORE_SIZE>EEPROM_ROM_SIZE
	#error Not enough EEPROM in the device!
#endif

#define EEPROM_START_MARKER		0xA5		// Marker byte (first byte for search).
#define EEPROM_CRC_POSITION		(EEPROM_STORE_SIZE-1)	// Position (offset from the start) of the CRC byte.
#define EEPROM_ERASED_DATA		0x00		// EEPROM cell state after erasing (inverted).

#define EEPROM_OK			0			// Everything went fine.
#define EEPROM_NO_DATA		1			// No valid data found.

// HAL
#if defined(__AVR_ATmega32__)	// ATmega32(A)
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEWE))
#define WAIT_SELF_WR
#define EEP_SET_WR1
#define EEP_SET_WR2
#define EEP_SET_ER1
#define EEP_SET_ER2
#define EEP_PREP_WRITE	EECR|=(1<<EEMWE)
#define EEP_START_WRITE	EECR|=(1<<EEWE)
#define EEP_START_READ	EECR|=(1<<EERE)
#define EEP_NO_ERASE	1
#endif

#if defined(__AVR_ATmega32A__)	// ATmega32(A)
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEWE))
#define WAIT_SELF_WR
#define EEP_SET_WR1
#define EEP_SET_WR2
#define EEP_SET_ER1
#define EEP_SET_ER2
#define EEP_PREP_WRITE	EECR|=(1<<EEMWE)
#define EEP_START_WRITE	EECR|=(1<<EEWE)
#define EEP_START_READ	EECR|=(1<<EERE)
#define EEP_NO_ERASE	1
#endif

#if defined(__AVR_ATmega48__)	// ATmega48
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined(__AVR_ATmega48A__)	// ATmega48A
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined(__AVR_ATmega48P__)	// ATmega48P
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	_SFR_IO16(0x21)
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined(__AVR_ATmega48PA__)	// ATmega48PA
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	_SFR_IO16(0x21)
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined(__AVR_ATmega88__)	// ATmega88
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined(__AVR_ATmega88A__)	// ATmega88A
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined(__AVR_ATmega88P__)	// ATmega88P
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined(__AVR_ATmega88PA__)	// ATmega88PA
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined (__AVR_ATmega168__) // ATmega168
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined (__AVR_ATmega168A__) // ATmega168A
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined (__AVR_ATmega168P__) // ATmega168P
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined (__AVR_ATmega168PA__) // ATmega168PA
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SPMEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined (__AVR_ATmega328__) // ATmega328
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SELFPRGEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#if defined (__AVR_ATmega328P__) // ATmega328P
#define EEP_DATA_REG	EEDR
#define EEP_ADDR_REG	EEAR
#define WAIT_EEP_WR		while(EECR&(1<<EEPE))
#define WAIT_SELF_WR	while(SPMCSR&(1<<SELFPRGEN))
#define EEP_SET_WR1		EECR&=~(1<<EEPM0)
#define EEP_SET_WR2		EECR|=(1<<EEPM1)
#define EEP_SET_ER1		EECR|=(1<<EEPM0)
#define EEP_SET_ER2		EECR&=~(1<<EEPM1)
#define EEP_PREP_WRITE	EECR|=(1<<EEMPE)|(0<<EEPE)
#define EEP_START_WRITE	EECR|=(1<<EEPE)
#define EEP_START_READ	EECR|=(1<<EERE)
#undef EEP_NO_ERASE
#endif

#ifdef EEP_16BIT_ADDR
void EEPROM_erase_byte(const uint16_t *, uint16_t);
void EEPROM_erase_byte_intfree(const uint16_t *, uint16_t);
void EEPROM_write_byte(const uint16_t *, uint16_t, uint8_t);
void EEPROM_write_byte_intfree(const uint16_t *, uint16_t, uint8_t);
void EEPROM_read_byte(const uint16_t *, uint16_t, uint8_t *);
uint8_t EEPROM_calc_CRC(void);
uint8_t EEPROM_search_data(uint8_t *, uint16_t, uint16_t);
void EEPROM_read_segment(uint8_t *, uint16_t, uint16_t);
void EEPROM_write_segment(uint8_t *, uint16_t, uint16_t);
void EEPROM_write_segment_intfree(uint8_t *, uint16_t, uint16_t);
#else
void EEPROM_erase_byte(const uint16_t *, uint8_t);
void EEPROM_erase_byte_intfree(const uint16_t *, uint8_t);
void EEPROM_write_byte(const uint16_t *, uint8_t, uint8_t);
void EEPROM_write_byte_intfree(const uint16_t *, uint8_t, uint8_t);
void EEPROM_read_byte(const uint16_t *, uint8_t, uint8_t *);
uint8_t EEPROM_calc_CRC(void);
uint8_t EEPROM_search_data(uint8_t *, uint8_t, uint8_t);
void EEPROM_read_segment(uint8_t *, uint8_t, uint8_t);
void EEPROM_write_segment(uint8_t *, uint8_t, uint8_t);
void EEPROM_write_segment_intfree(uint8_t *, uint8_t, uint8_t);
#endif	/*EEP_16BIT_ADDR*/
void EEPROM_goto_next_segment(void);

#endif /* DRV_EEPROM_H_ */
