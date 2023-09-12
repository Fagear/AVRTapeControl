#include "common_log.h"

// ~2 ticks enough for SOLENOID
// ~30 ticks STOP -> PLAY
// ~50 ticks FWIND -> STOP
// STOP-skip PLAY: 255...[70...50] ~60
// PLAY-skip FWIND: 255...[120...100] ~110
// FWIND-skip STOP: 255...[205...180] ~200
// PLAY-select REWIND: 255...[230...210] ~225
#define TIM_TANA_DLY_SW_ACT			5		// 10 ms (time for solenoid activation and command gear to start turning)
#define TIM_TANA_DLY_WAIT_REW_ACT	30		// 60 ms (time for second solenoid activation for selecting rewind)
#define TIM_TANA_DLY_PB_WAIT		195		// 390 ms (time from the first solenoid activation in STOP until PLAY is fully selected)
#define TIM_TANA_DLY_FWIND_WAIT		145		// 290 ms (time from the first solenoid activation in PLAY until FWIND is fully selected)
#define TIM_TANA_DELAY_STOP			240		//

// TODO: set timeouts
#define TACHO_TANA_STOP_DLY_MAX		200
#define TACHO_TANA_PLAY_DLY_MAX		200
#define TACHO_TANA_FWIND_DLY_MAX	200

// States of Tanashin mechanism for [u8_tanashin_target_mode] and [u8_tanashin_mode] (including "SUBMODES").
enum
{
	TTR_TANA_MODE_TO_INIT,			// Start-up state
	TTR_TANA_MODE_INIT,				// Wait for mechanism to stabilize upon power-up
	TTR_TANA_SUBMODE_TO_STOP,		// Start transition to STOP
	TTR_TANA_MODE_STOP,				// Stable STOP state
	TTR_TANA_MODE_PB_FWD,			// Stable PLAYBACK in forward direction
	TTR_TANA_MODE_FW_FWD,			// Stable FAST WIND in forward direction
	TTR_TANA_MODE_FW_REV,			// Stable FAST WIND in reverse direction
	TTR_TANA_MODE_TO_HALT,			// Start transition to HALT
	TTR_TANA_MODE_HALT,				// Permanent HALT due to an error
	TTR_TANA_MODE_MAX				// Mode selector limit
};

extern volatile const uint8_t ucaf_tanashin_mech[];

uint8_t mech_tanashin_user_to_transport(uint8_t in_mode);				// Convert user mode to transport mode for Tanashin-clone mechanism
void mech_tanashin_static_halt(uint8_t in_sws, uint8_t *usr_mode);		// Transport operations are halted, keep mechanism in this state
void mech_tanashin_target2mode(uint8_t *tacho, uint8_t *usr_mode);		// Start transition from current mode to target mode
void mech_tanashin_user2target(uint8_t *usr_mode);						// Take in user desired mode and set new target mode
void mech_tanashin_static_mode(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir);		// Control mechanism in static mode (not transitioning between modes)
void mech_tanashin_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir);	// Perform tape transport state machine for Tanashin-clone tape mech
uint8_t mech_tanashin_get_mode();										// Get user-level mode of the transport
uint8_t mech_tanashin_get_transition();									// Get transition timer count
uint8_t mech_tanashin_get_error();										// Get transport error
void mech_tanashin_UART_dump_mode(uint8_t in_mode);						// Print Tanashin transport mode alias
