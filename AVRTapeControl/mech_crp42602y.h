/**************************************************************************************************************************************************************
mech_crp42602y.h

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
State machine for CRP42602Y tape transport (TTR) mechanism from AliExpress. Looks like some mechanisms used in LG tape machines.
Belt-driven, one motor (for capstan, take-up and mode switching), one solenoid (for mode switching).
This mechanism has two capstans and supports reverse operations.
Also this mechanism has takeup tachometer sensor on drive pulley and it pulses in STOP.

**************************************************************************************************************************************************************/

#include "common_log.h"

// Timer marks for various modes for CRP42602Y mechanism, contained in [u8_crp42602y_trans_timer].
// Each tick = 2 ms real time.
#define TIM_42602_DLY_STOP			23		// 46 ms  (end of starting pulse of transition to STOP)
#define TIM_42602_DLY_WAIT_HEAD		12		// 24 ms  (end of starting pulse of transition to ACTIVE, start of "gray zone")
#define TIM_42602_DLY_HEAD_DIR		24		// 48 ms  (start of head/pinch direction selection range)
#define TIM_42602_DLY_WAIT_PINCH	52		// 104 ms (end of head/pinch direction selection range, start of "gray zone")
#define TIM_42602_DLY_PINCH_EN		73		// 146 ms (start of pinch engage selection range)
#define TIM_42602_DLY_WAIT_TAKEUP	128		// 256 ms (end of pinch engage selection range, start of "gray zone")
#define TIM_42602_DLY_TAKEUP_DIR	144		// 288 ms (start of takeup direction selection range)
#define TIM_42602_DLY_WAIT_MODE		184		// 368 ms (end of takeup direction selection range, waiting for transition to active mode)
#define TIM_42602_DLY_ACTIVE		210		// 420 ms (time for full transition STOP -> ACTIVE)
#define TIM_42602_DLY_WAIT_STOP		160		// 320 ms (time for full transition ACTIVE -> STOP)

// Maximum wait for next tacho tick for various modes, contained in [u8_tacho_timer].
// Each tick = 20 ms real time.
#define TACHO_42602_STOP_DLY_MAX	12		// 240 ms (3.7 Hz)
#define TACHO_42602_PLAY_DLY_MAX	50		// 1000 ms (1.1...2.6 Hz)
#define TACHO_42602_FWIND_DLY_MAX	10		// 120 ms (19.5...21 Hz)

// States of CRP42602Y mechanism for [u8_crp42602y_target_mode] and [u8_crp42602y_mode] (including "SUBMODES").
enum
{
	TTR_42602_MODE_TO_INIT,			// Start-up state
	TTR_42602_SUBMODE_INIT,			// Wait for mechanism to stabilize upon power-up
	TTR_42602_SUBMODE_TO_STOP,		// Start transition to STOP
	TTR_42602_SUBMODE_WAIT_STOP,	// Waiting for mechanism to reach STOP sensor
	TTR_42602_MODE_STOP,			// Stable STOP state
	TTR_42602_SUBMODE_TO_ACTIVE,	// Start transition from STOP to any active mode
	TTR_42602_SUBMODE_ACT,			// Waiting for main cyclogram to start
	TTR_42602_SUBMODE_WAIT_DIR,		// Waiting for pinch direction change range
	TTR_42602_SUBMODE_HD_DIR_SEL,	// Head/pinch direction selection range
	TTR_42602_SUBMODE_WAIT_PINCH,	// Waiting for pinch engage range
	TTR_42602_SUBMODE_PINCH_SEL,	// Choose to engage pinch roller
	TTR_42602_SUBMODE_WAIT_TAKEUP,	// Waiting for takeup direction change range
	TTR_42602_SUBMODE_TU_DIR_SEL,	// Takeup direction selection range
	TTR_42602_SUBMODE_WAIT_RUN,		// Waiting for mechanism to stabilize
	TTR_42602_MODE_PB_FWD,			// Stable PLAYBACK in forward direction
	TTR_42602_MODE_PB_REV,			// Stable PLAYBACK in reverse direction
	TTR_42602_MODE_RC_FWD,			// Stable RECORD in forward direction
	TTR_42602_MODE_RC_REV,			// Stable RECORD in reverse direction
	TTR_42602_MODE_FW_FWD,			// Stable FAST WIND in forward direction, head/pinch in forward direction
	TTR_42602_MODE_FW_REV,			// Stable FAST WIND in reverse direction, head/pinch in forward direction
	TTR_42602_MODE_FW_FWD_HD_REV,	// Stable FAST WIND in forward direction, head/pinch in reverse direction
	TTR_42602_MODE_FW_REV_HD_REV,	// Stable FAST WIND in reverse direction, head/pinch in reverse direction
	TTR_42602_SUBMODE_TO_HALT,		// Start transition to HALT
	TTR_42602_MODE_HALT,			// Permanent HALT due to an error
	TTR_42602_MODE_MAX				// Mode selector limit
};

extern volatile const uint8_t ucaf_crp42602y_mech[];

void mech_crp42602y_set_error(uint8_t in_err);							// Freeze transport due to error
uint8_t mech_crp42602y_user_to_transport(uint8_t in_mode, uint8_t *play_dir);		// Convert user mode to transport mode
void mech_crp42602y_static_halt(uint8_t in_sws, uint8_t *usr_mode);		// Transport operations are halted, keep mechanism in this state
void mech_crp42602y_target2mode(uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode);	// Start transition from current mode to target mode
void mech_crp42602y_user2target(uint8_t *usr_mode, uint8_t *play_dir);	// Take in user desired mode and set new target mode
void mech_crp42602y_static_mode(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir);	// Control mechanism in static mode (not transitioning between modes)
void mech_crp42602y_cyclogram(uint8_t in_sws, uint8_t *play_dir);		// Transition through modes, timing solenoid
void mech_crp42602y_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir);	// Perform tape transport state machine
uint8_t mech_crp42602y_get_mode();										// Get user-level mode of the transport
uint8_t mech_crp42602y_get_transition();								// Get transition timer count
uint8_t mech_crp42602y_get_error();										// Get transport error
void mech_crp42602y_UART_dump_mode(uint8_t in_mode);					// Print transport mode alias
