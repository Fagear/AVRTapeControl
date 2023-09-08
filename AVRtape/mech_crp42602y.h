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
#define TIM_42602_DELAY_RUN			200		// 400 ms (time for full transition STOP -> ACTIVE)
#define TIM_42602_DELAY_STOP		200		// 400 ms (time for full transition ACTIVE -> STOP)

// Maximum wait for next tacho tick for various modes, contained in [u8_tacho_timer].
// Each tick = 20 ms real time.
#define TACHO_42602_STOP_DLY_MAX	12		// 240 ms (~4 Hz)
#define TACHO_42602_PLAY_DLY_MAX	50		// 1000 ms (~1 Hz)
#define TACHO_42602_FWIND_DLY_MAX	10		// 120 ms (~8 Hz)

// States of CRP42602Y mechanism for [u8_crp42602y_target_mode] and [u8_crp42602y_mode] (including "SUBMODES").
enum
{
	TTR_42602_MODE_TO_INIT,			// 0 Start-up state
	TTR_42602_SUBMODE_INIT,			// 1 Wait for mechanism to stabilize upon power-up
	TTR_42602_SUBMODE_TO_STOP,		// 2 Start transition from active mode to STOP
	TTR_42602_SUBMODE_WAIT_STOP,	// 3 Waiting for mechanism to reach STOP sensor
	TTR_42602_MODE_STOP,			// 4 Stable STOP state
	TTR_42602_SUBMODE_TO_START,		// 5 Start transition from STOP to any active mode
	TTR_42602_SUBMODE_WAIT_DIR,		// 6 Waiting for pinch direction change range
	TTR_42602_SUBMODE_HD_DIR_SEL,	// 7 Head/pinch direction selection range
	TTR_42602_SUBMODE_WAIT_PINCH,	// 8 Waiting for pinch engage range
	TTR_42602_SUBMODE_PINCH_SEL,	// 9 Choose to engage pinch roller
	TTR_42602_SUBMODE_WAIT_TAKEUP,	// 10 Waiting for takeup direction change range
	TTR_42602_SUBMODE_TU_DIR_SEL,	// 11 Takeup direction selection range
	TTR_42602_SUBMODE_WAIT_RUN,		// 12 Waiting for mechanism to stabilize
	TTR_42602_MODE_PB_FWD,			// 13 Stable PLAYBACK in forward direction
	TTR_42602_MODE_PB_REV,			// 14 Stable PLAYBACK in reverse direction
	TTR_42602_MODE_FW_FWD,			// 15 Stable FAST WIND in forward direction, head/pinch in forward direction
	TTR_42602_MODE_FW_REV,			// 16 Stable FAST WIND in reverse direction, head/pinch in forward direction
	TTR_42602_MODE_FW_FWD_HD_REV,	// 17 Stable FAST WIND in forward direction, head/pinch in reverse direction
	TTR_42602_MODE_FW_REV_HD_REV,	// 18 Stable FAST WIND in reverse direction, head/pinch in reverse direction
	TTR_42602_MODE_HALT,			// 19 Permanent halt due to an error
	TTR_42602_MODE_MAX				// 20 Mode selector limit
};

uint8_t mech_crp42602y_user_to_transport(uint8_t in_mode);
void mech_crp42602y_static_halt(uint8_t in_sws, uint8_t *usr_mode);
void mech_crp42602y_target2mode(uint8_t *usr_mode);
void mech_crp42602y_user2target(uint8_t *usr_mode);
void mech_crp42602y_static_mode(uint8_t in_features, uint8_t in_sws, uint8_t *taho, uint8_t *usr_mode, uint8_t *play_dir);
void mech_crp42602y_cyclogram();
void mech_crp42602y_state_machine(uint8_t in_features, uint8_t in_sws, uint8_t *taho, uint8_t *usr_mode, uint8_t *play_dir);
uint8_t mech_crp42602y_get_mode();
uint8_t mech_crp42602y_get_transition();
uint8_t mech_crp42602y_get_error();
void mech_crp42602y_UART_dump_mode(uint8_t in_mode);
