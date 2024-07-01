/**************************************************************************************************************************************************************
mech_tanashin.h

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

Created: 2023-09-07

State machine for typical Tanashin-clone tape transport (TTR) mechanism with logical/software control.
One motor belt-driven (for capstan, take-up and mode switching), one solenoid (for mode switching).
This mechanism does not support reverse operations.

TODO: still work in progress, does not work!
TODO: recording doesn't start and hangs state machine

**************************************************************************************************************************************************************/

#include "common_log.h"

// ~2 ticks enough for SOLENOID
// ~30 ticks STOP -> PLAY
// ~50 ticks FWIND -> STOP
// STOP-skip PLAY: 255...[70...50] ~60
// PLAY-skip FWIND: 255...[120...100] ~110
// FWIND-skip STOP: 255...[205...180] ~200
// PLAY-select REWIND: 255...[230...210] ~225
#define TIM_TANA_DLY_SW_ACT			8		// 16 ms (time for solenoid activation and command gear to start turning)
#define TIM_TANA_DLY_WAIT_REW_ACT	35		// 70 ms (time for second solenoid activation for selecting rewind)
#define TIM_TANA_DLY_FWIND_ACT		85		// 170 ms (time for solenoid deactivation after selecting rewind)
#define TIM_TANA_DLY_PB_WAIT		210		// 420 ms (time from the first solenoid activation in STOP until PLAY is fully selected)
#define TIM_TANA_DLY_FWIND_WAIT		155		// 310 ms (time from the first solenoid activation in PLAY until FWIND is fully selected)
#define TIM_TANA_DLY_STOP			80		// 160 ms (time from the first solenoid activation in FWIND until STOP is fully selected)
#define TIM_TANA_DLY_ACTIVE			240		// 480 ms (time for initial stabilization/maximum mode change)

// Maximum wait for next tacho tick for various modes, contained in [u8_tacho_timer].
// Each tick = 20 ms real time.
#define TACHO_TANA_PLAY_DLY_MAX		65		// 1300 ms (1...3 Hz)
#define TACHO_TANA_FWIND_DLY_MAX	5		// 100 ms (13...56 Hz)

// Maximum wait before capstan shutdown.
#define IDLE_TANA_NO_TAPE			7500	// 15 s
#define IDLE_TANA_TAPE_IN			60000	// 120 s
#define IDLE_TANA_MAX				IDLE_TANA_TAPE_IN

// Maximum number of tries to switch to STOP mode before HALT.
#define REP_TANA_MAX				4

// Power for tachometer through absent reverse record inhibit switch pin.
#define TANA_TACHO_PWR_SETUP		(SW_DIR|=SW_NOREC_REV)
#define TANA_TACHO_PWR_EN			(SW_PORT|=SW_NOREC_REV)
#define TANA_TACHO_PWR_DIS			(SW_PORT&=~SW_NOREC_REV)

// States of Tanashin mechanism for [u8_tanashin_target_mode] and [u8_tanashin_mode] (including "SUBMODES").
enum
{
	TTR_TANA_MODE_TO_INIT,			// Start-up state
	TTR_TANA_MODE_INIT,				// Wait for mechanism to stabilize upon power-up
	TTR_TANA_SUBMODE_TO_STOP,		// Start transition to STOP
	TTR_TANA_SUBMODE_WAIT_STOP,		// Waiting for mechanism to reach STOP sensor
	TTR_TANA_MODE_STOP,				// Stable STOP state
	TTR_TANA_SUBMODE_TO_PLAY,		// Start transition to PLAYBACK
	TTR_TANA_SUBMODE_WAIT_PLAY,		// Waiting for mechanism to transition to PLAYBACK
	TTR_TANA_MODE_PB_FWD,			// Stable PLAYBACK in forward direction
	TTR_TANA_SUBMODE_TO_REC,		// Start transition to RECORD
	TTR_TANA_SUBMODE_WAIT_REC,		// Waiting for mechanism to transition to RECORD
	TTR_TANA_MODE_RC_FWD,			// Stable RECORD in forward direction
	TTR_TANA_SUBMODE_TO_FWIND,		// Start transition to FAST WIND
	TTR_TANA_SUBMODE_WAIT_TAKEUP,	// Waiting for takeup direction change range
	TTR_TANA_SUBMODE_TU_DIR_SEL,	// Takeup direction selection range
	TTR_TANA_SUBMODE_WAIT_FWIND,	// Waiting for mechanism to transition to FAST WIND
	TTR_TANA_MODE_FW_FWD,			// Stable FAST WIND in forward direction
	TTR_TANA_MODE_FW_REV,			// Stable FAST WIND in reverse direction
	TTR_TANA_SUBMODE_TO_HALT,		// Start transition to HALT
	TTR_TANA_MODE_HALT,				// Permanent HALT due to an error
	TTR_TANA_MODE_MAX				// Mode selector limit
};

extern volatile const uint8_t ucaf_tanashin_mech[];

void mech_tanashin_set_error(uint8_t in_err);							// Freeze transport due to error.
uint8_t mech_tanashin_user_to_transport(uint8_t in_mode);				// Convert user mode to transport mode for Tanashin-clone mechanism
void mech_tanashin_static_halt(uint8_t in_sws, uint8_t *usr_mode);		// Transport operations are halted, keep mechanism in this state
void mech_tanashin_target2mode(uint8_t *tacho, uint8_t *usr_mode);		// Start transition from current mode to target mode
void mech_tanashin_user2target(uint8_t *usr_mode);						// Take in user desired mode and set new target mode
void mech_tanashin_static_mode(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode);						// Control mechanism in static mode (not transitioning between modes)
void mech_tanashin_cyclogram(uint8_t in_sws);							// Transition through modes, timing solenoid
void mech_tanashin_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir);	// Perform tape transport state machine for Tanashin-clone tape mech
uint8_t mech_tanashin_get_mode();										// Get user-level mode of the transport
uint8_t mech_tanashin_get_transition();									// Get transition timer count
uint8_t mech_tanashin_get_error();										// Get transport error
void mech_tanashin_UART_dump_mode(uint8_t in_mode);						// Print Tanashin transport mode alias
