/*
 * common_log.h
 *
 * Created: 07.09.2023 17:14:38
 *  Author: kryukov
 */


#ifndef COMMON_LOG_H_
#define COMMON_LOG_H_

#include <stdio.h>
#include "config.h"
#include "drv_io.h"
#include "strings.h"

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

// Flags for [sw_state], [sw_pressed] and [sw_released].
#define TTR_SW_TAPE_IN		(1<<0)	// Tape is present
#define TTR_SW_STOP			(1<<1)	// Tape transport in mechanical "STOP" mode
#define TTR_SW_TACHO		(1<<2)	// Tape pickup tachometer
#define TTR_SW_NOREC_FWD	(1<<3)	// Rec inhibit in forward direction
#define TTR_SW_NOREC_REV	(1<<4)	// Rec inhibit in reverse direction

#define SLEEP_INHIBIT_2HZ	6		// Time for sleep inhibition with 2HZ rate

// State of tape playback direction.
enum
{
	PB_DIR_FWD,						// Forward playback
	PB_DIR_REV						// Reverse playback
};

enum
{
	TTR_TYPE_CRP42602Y,				// CRP42602Y mechanism from AliExpress
	TTR_TYPE_TANASHIN				// Tanashin-clone mechanism from AliExpress
};

// User-selectable modes for [u8_user_mode].
enum
{
	USR_MODE_STOP,					// STOP
	USR_MODE_PLAY_FWD,				// PLAY in forward
	USR_MODE_PLAY_REV,				// PLAY in reverse
	USR_MODE_REC_FWD,				// RECORD in forward
	USR_MODE_REC_REV,				// RECORD in reverse
	USR_MODE_FWIND_FWD,				// FAST WIND in forward
	USR_MODE_FWIND_REV				// FAST WIND in reverse
};

// Error flags of the transport for [u8_transport_error].
enum
{
	TTR_ERR_NONE = 0x00,			// No error registered
	TTR_ERR_TAPE_LOST = 0x01,		// Tape was removed during operation
	TTR_ERR_BAD_DRIVE = 0x02,		// No tacho in stop, probably bad belts or motor stall
	TTR_ERR_NO_CTRL = 0x04,			// Unable to transition through modes, probably low power to solenoid, bad solenoid or actuator jammed
	TTR_ERR_LOGIC_FAULT = 0x80		// Abnormal logic state detected
};

// Flags for reverse playback settings for [u16_features].
enum
{
	TTR_FEA_STOP_TACHO = (1<<0),	// Enable checking tachometer in STOP mode (some CRP42602Y can do that)
	TTR_FEA_REV_ENABLE = (1<<1),	// Enable all operations with reverse playback (does affect [TTR_FEA_PB_AUTOREV] and [TTR_FEA_PB_LOOP])
	TTR_FEA_PB_AUTOREV = (1<<2),	// Enable auto-reverse for forward playback (PB FWD -> PB REV -> STOP)
	TTR_FEA_PB_LOOP = (1<<3),		// Enable full auto-reverse (PB FWD -> PB REV -> PB FWD -> ...)
	TTR_FEA_END_REW = (1<<4),		// Enable auto-rewind for fast forward (PB FWD/FW FWD -> FW REV -> STOP) (lower priority than [TTR_FEA_PB_LOOP])
	TTR_FEA_TWO_PLAYS = (1<<5),		// Enable two PLAY buttons/LEDs (for each direction)
};
//#define TTR_REV_DEFAULT		(TTR_FEA_STOP_TACHO|TTR_FEA_REV_ENABLE|TTR_FEA_PB_AUTOREV|TTR_FEA_END_REW)		// Default reverse mode settings
#define TTR_REV_DEFAULT		(TTR_FEA_STOP_TACHO|TTR_FEA_REV_ENABLE|TTR_FEA_PB_AUTOREV|TTR_FEA_TWO_PLAYS)		// Default reverse mode settings

// Index of byte in SPI bus extenders for [u8a_spi_buf].
enum
{
	SPI_IDX_IND,					// Regular transport mode indicators
	SPI_IDX_MAX						// Index limit
};

void UART_dump_user_mode(uint8_t in_mode);
void UART_dump_settings(uint16_t in_settings);

#endif /* COMMON_LOG_H_ */