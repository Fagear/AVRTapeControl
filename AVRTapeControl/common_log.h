/**************************************************************************************************************************************************************
common_log.h

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

Created: 2023-09-07

Part of the [AVRTapeControl] project.
Common logic defines for tape transport processing and enabled features.

**************************************************************************************************************************************************************/

#ifndef COMMON_LOG_H_
#define COMMON_LOG_H_

#include <stdio.h>
#include "drv_io.h"

// Flags for [sw_state], [sw_pressed] and [sw_released].
#define TTR_SW_TAPE_IN		(1<<0)	// Tape is present
#define TTR_SW_STOP			(1<<1)	// Tape transport in mechanical "STOP" mode
#define TTR_SW_TACHO		(1<<2)	// Tape pickup tachometer
#define TTR_SW_NOREC_FWD	(1<<3)	// Rec inhibit in forward direction
#define TTR_SW_NOREC_REV	(1<<4)	// Rec inhibit in reverse direction

// Maximum wait before capstan shutdown.
#define IDLE_CAP_NO_TAPE			7500	// 15 s
#define IDLE_CAP_TAPE_IN			60000	// 120 s
#define IDLE_CAP_MAX				IDLE_CAP_TAPE_IN

// Maximum number of tries to switch mode.
#define MODE_REP_MAX				6

// State of tape playback direction.
enum
{
	PB_DIR_FWD,						// Forward playback
	PB_DIR_REV						// Reverse playback
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
	TTR_ERR_NONE		= 0x00,		// No error registered
	TTR_ERR_TAPE_LOST	= 0x01,		// Tape was removed during operation
	TTR_ERR_BAD_DRIVE	= 0x02,		// No tacho in stop, probably bad belts or motor stall
	TTR_ERR_NO_CTRL		= 0x04,		// Unable to transition through modes, probably low power to solenoid, bad solenoid or actuator jammed
	TTR_ERR_LOGIC_FAULT = 0x80,		// Abnormal logic state detected
};

// Flags for transport features for [u8a_settings[EPS_TTR_FTRS]].
enum
{
	TTR_FEA_STOP_TACHO	= (1<<0),	// Enable checking tachometer in STOP mode (some CRP42602Y can do that)
	TTR_FEA_REV_ENABLE	= (1<<1),	// Enable all operations with reverse playback (does affect [SRV_FEA_PB_AUTOREV] and [SRV_FEA_PB_LOOP])
};

// Flags for service features for [u8a_settings[EPS_SRV_FTRS]].
enum
{
	SRV_FEA_TWO_PLAYS	= (1<<0),	// Enable two PLAY buttons/LEDs (for each direction)
	SRV_FEA_ONE2REC		= (1<<1),	// Enable one-button record start (no need to press any PLAY button)
	SRV_FEA_PB_AUTOREV	= (1<<2),	// Enable auto-reverse for forward playback (PB FWD -> PB REV -> STOP)
	SRV_FEA_PB_LOOP		= (1<<3),	// Enable full auto-reverse (PB FWD -> PB REV -> PB FWD -> ...)
	SRV_FEA_PBF2REW		= (1<<4),	// Enable auto-rewind for forward PLAY (PB FWD -> FW REV -> STOP) (lower priority than [SRV_FEA_PB_LOOP])
	SRV_FEA_FF2REW		= (1<<5),	// Enable auto-rewind for fast forward (FW FWD -> FW REV -> STOP)
};

void UART_dump_user_mode(uint8_t in_mode);

#endif /* COMMON_LOG_H_ */
