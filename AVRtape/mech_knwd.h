/**************************************************************************************************************************************************************
mech_knwd.h

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

Created: 2024-07-09

State machine for tape transport (TTR) mechanism used in Kenwood Mini Hi-Fis. Like in RXD-500~760.
Belt-driven, one motor (for capstan, take-up and mode switching), one solenoid (for mode switching).
This mechanism has two capstans and supports reverse operations.

**************************************************************************************************************************************************************/

#include "common_log.h"

// Timer marks for various modes for Kenwood mechanism, contained in [u8_knwd_trans_timer].
// Each tick = 2 ms real time.
// TODO
#define TIM_KNWD_DLY_SW_ACT			14		// TODO
#define TIM_KNWD_DLY_STOP			23		// TODO (end of starting pulse of transition to STOP)
#define TIM_KNWD_DLY_PB_WAIT		240		// TODO
#define TIM_KNWD_DLY_FWIND_WAIT		240		// TODO
#define TIM_KNWD_DLY_ACTIVE			240		// TODO
#define TIM_KNWD_DLY_WAIT_STOP		240		// TODO

// Maximum wait for next tacho tick for various modes, contained in [u8_tacho_timer].
// Each tick = 20 ms real time.
#define TACHO_KNWD_STOP_DLY_MAX		240		// TODO
#define TACHO_KNWD_PLAY_DLY_MAX		240		// TODO
#define TACHO_KNWD_FWIND_DLY_MAX	240		// TODO

// States of Kenwood mechanism for [u8_knwd_target_mode] and [u8_knwd_mode] (including "SUBMODES").
enum
{
	TTR_KNWD_MODE_TO_INIT,			// Start-up state
	TTR_KNWD_SUBMODE_INIT,			// Wait for mechanism to stabilize upon power-up
	TTR_KNWD_SUBMODE_TO_STOP,		// Start transition from active mode to STOP
	TTR_KNWD_SUBMODE_WAIT_STOP,		// Waiting for mechanism to reach STOP sensor
	TTR_KNWD_MODE_STOP,				// Stable STOP state
	TTR_KNWD_SUBMODE_TO_PLAY,		// Start transition to PLAYBACK
	TTR_KNWD_SUBMODE_WAIT_PLAY,		// Waiting for mechanism to transition to PLAYBACK
	TTR_KNWD_MODE_PB_FWD,			// Stable PLAYBACK in forward direction
	TTR_KNWD_MODE_PB_REV,			// Stable PLAYBACK in reverse direction
	TTR_KNWD_MODE_RC_FWD,			// Stable RECORD in forward direction
	TTR_KNWD_MODE_RC_REV,			// Stable RECORD in reverse direction
	TTR_KNWD_SUBMODE_TO_FWIND,		// Start transition to FAST WIND
	TTR_KNWD_SUBMODE_WAIT_FWIND,	// Waiting for mechanism to transition to FAST WIND
	TTR_KNWD_MODE_FW_FWD,			// Stable FAST WIND in forward direction, head/pinch in forward direction
	TTR_KNWD_MODE_FW_REV,			// Stable FAST WIND in reverse direction, head/pinch in forward direction
	TTR_KNWD_MODE_FW_FWD_HD_REV,	// Stable FAST WIND in forward direction, head/pinch in reverse direction
	TTR_KNWD_MODE_FW_REV_HD_REV,	// Stable FAST WIND in reverse direction, head/pinch in reverse direction
	TTR_KNWD_SUBMODE_TO_HALT,		// Start transition to HALT
	TTR_KNWD_MODE_HALT,				// Permanent HALT due to an error
	TTR_KNWD_MODE_MAX				// Mode selector limit
};

extern volatile const uint8_t ucaf_knwd_mech[];

void mech_knwd_set_error(uint8_t in_err);								// Freeze transport due to error
uint8_t mech_knwd_user_to_transport(uint8_t in_mode, uint8_t *play_dir);// Convert user mode to transport mode
void mech_knwd_static_halt(uint8_t in_sws, uint8_t *usr_mode);			// Transport operations are halted, keep mechanism in this state
void mech_knwd_target2mode(uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode);	// Start transition from current mode to target mode
void mech_knwd_user2target(uint8_t *usr_mode, uint8_t *play_dir);		// Take in user desired mode and set new target mode
void mech_knwd_static_mode(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir);		// Control mechanism in static mode (not transitioning between modes)
void mech_knwd_cyclogram(uint8_t in_sws, uint8_t *play_dir);			// Transition through modes, timing solenoid
void mech_knwd_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir);	// Perform tape transport state machine
uint8_t mech_knwd_get_mode();											// Get user-level mode of the transport
uint8_t mech_knwd_get_transition();										// Get transition timer count
uint8_t mech_knwd_get_error();											// Get transport error
void mech_knwd_UART_dump_mode(uint8_t in_mode);							// Print transport mode alias
