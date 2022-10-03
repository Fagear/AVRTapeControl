/*
 * avrtape.h
 *
 * Created:			2021-04-13 14:16:02
 * Modified:		2021-04-16
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		Main header, defines for flags, enums and constants for core logic.
 *
 */ 

#ifndef AVRTAPE_H_
#define AVRTAPE_H_

#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include "drv_cpu.h"
#include "drv_eeprom.h"
#include "drv_io.h"
#include "strings.h"

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

// Flags for [kbd_state], [kbd_pressed] and [kbd_released].
#define USR_BTN_STOP		(1<<0)	// Stop button
#define USR_BTN_PLAY		(1<<1)	// Play/reverse direction button
#define USR_BTN_PLAY_REV	(1<<2)	// Play in reverse button
#define USR_BTN_REWIND		(1<<3)	// Rewind button
#define USR_BTN_FFORWARD	(1<<4)	// Fast forward button
#define USR_BTN_RECORD		(1<<5)	// Record button

// LED (?) indicators on the SPI 595 extender at [SPI_IDX_IND].
#define IND_TACHO			(1<<0)	// Tachometer indicator
#define IND_STOP			(1<<1)	// Stop indicator
#define IND_PLAY_FWD		(1<<2)  // Play in forward direction indicator
#define IND_PLAY_REV		(1<<3)	// Play in reverse direction indicator
#define IND_FFORWARD		(1<<4)	// Fast forward indicator
#define IND_REWIND			(1<<5)	// Rewind indicator
#define IND_REC				(1<<6)	// Record indicator
#define IND_ERROR			(1<<7)	// Transport error indicator

// Flags for [sw_state], [sw_pressed] and [sw_released].
#define TTR_SW_TAPE_IN		(1<<0)	// Tape is present
#define TTR_SW_STOP			(1<<1)	// Tape transport in mechanical "STOP" mode
#define TTR_SW_TACHO		(1<<2)	// Tape pickup tachometer
#define TTR_SW_NOREC_FWD	(1<<3)	// Rec inhibit in forward direction
#define TTR_SW_NOREC_REV	(1<<4)	// Rec inhibit in reverse direction

// Timer marks for various modes for CRP42602Y mechanism, contained in [u8_cycle_timer].
// Each tick = 2 ms real time.
#define TIM_42602_DLY_STOP			23		// 46 ms  (end of starting pulse of transition to STOP)
#define TIM_42602_DLY_WAIT_HEAD		12		// 24 ms  (end of starting pulse of transition to ACTIVE, start of "gray zone")
#define TIM_42602_DLY_HEAD_DIR		24		// 48 ms  (start of head/pinch direction selection range)
#define TIM_42602_DLY_WAIT_PINCH	52		// 104 ms (end of head/pinch direction selection range, start of "gray zone")
#define TIM_42602_DLY_PINCH_EN		73		// 146 ms (start of pinch engage selection range)
#define TIM_42602_DLY_WAIT_TAKEUP	128		// 256 ms (end of pinch engage selection range, start of "gray zone")
#define TIM_42602_DLY_TAKEUP_DIR	144		// 288 ms (start of takeup direction selection range)
#define TIM_42602_DLY_WAIT_MODE		184		// 368 ms (end of takeup direction selection range, waiting for transition to active mode)
#define TTR_42602_DELAY_RUN			200		// 400 ms (time for full transition STOP -> ACTIVE)
#define TTR_42602_DELAY_STOP		200		// 400 ms (time for full transition ACTIVE -> STOP)

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

// Maximum wait for next tacho tick for various modes, contained in [u8_tacho_timer].
// Each tick = 20 ms real time.
#define TACHO_42602_STOP_DLY_MAX	12		// 240 ms (~4 Hz)
#define TACHO_42602_PLAY_DLY_MAX	50		// 1000 ms (~1 Hz)
#define TACHO_42602_FWIND_DLY_MAX	10		// 120 ms (~8 Hz)

// TODO: set timeouts
#define TACHO_TANA_STOP_DLY_MAX		200		// 240 ms (~4 Hz)
#define TACHO_TANA_PLAY_DLY_MAX		200		// 1000 ms (~1 Hz)
#define TACHO_TANA_FWIND_DLY_MAX	200		// 120 ms (~8 Hz)


enum
{
	FALSE,
	TRUE
};

enum
{
	TTR_TYPE_CRP42602Y,			// Mechanism CRP42602Y from AliExpress
	TTR_TYPE_TANASHIN			// Mechanism Tanashin-clone from AliExpress
};

// State of tape playback direction.
enum
{
	PB_DIR_FWD,						// Forward
	PB_DIR_REV						// Reverse
};

// User-selectable modes.
enum
{
	USR_MODE_STOP,					// STOP
	USR_MODE_PLAY_FWD,				// PLAY forward
	USR_MODE_PLAY_REV,				// PLAY in reverse
	USR_MODE_FWIND_FWD,				// FAST WIND forward
	USR_MODE_FWIND_REV				// FAST WIND in reverse
};

// States of CRP42602Y mechanism for [u8_transport_mode], [u8_target_trr_mode] and [u8_user_mode].
enum
{
	TTR_42602_MODE_TO_INIT,			// 0 Start-up state
	TTR_42602_MODE_INIT,			// 1 Wait for mechanism to stabilize upon power-up
	TTR_42602_MODE_STOP,			// 2 Stable STOP state
	TTR_42602_MODE_TO_START,		// 3 Start transition from STOP to any active mode
	TTR_42602_MODE_WAIT_DIR,		// 4 Waiting for pinch direction change range
	TTR_42602_MODE_HD_DIR_SEL,		// 5 Head/pinch direction selection range
	TTR_42602_MODE_WAIT_PINCH,		// 6 Waiting for pinch engage range
	TTR_42602_MODE_PINCH_SEL,		// 7 Choose to engage pinch roller
	TTR_42602_MODE_WAIT_TAKEUP,		// 8 Waiting for takeup direction change range
	TTR_42602_MODE_TU_DIR_SEL,		// 9 Takeup direction selection range
	TTR_42602_MODE_WAIT_RUN,		// 10 Waiting for mechanism to stabilize
	TTR_42602_MODE_PB_FWD,			// 11 Stable PLAYBACK in forward direction
	TTR_42602_MODE_PB_REV,			// 12 Stable PLAYBACK in reverse direction
	TTR_42602_MODE_FW_FWD,			// 13 Stable FAST WIND in forward direction, head/pinch in forward direction
	TTR_42602_MODE_FW_REV,			// 14 Stable FAST WIND in reverse direction, head/pinch in forward direction
	TTR_42602_MODE_FW_FWD_HD_REV,	// 15 Stable FAST WIND in forward direction, head/pinch in reverse direction
	TTR_42602_MODE_FW_REV_HD_REV,	// 16 Stable FAST WIND in reverse direction, head/pinch in reverse direction
	TTR_42602_MODE_TO_STOP,			// 17 Start transition from active mode to STOP
	TTR_42602_MODE_WAIT_STOP,		// 18 Waiting for mechanism to reach STOP sensor
	TTR_42602_MODE_HALT,			// 19 Permanent halt due to an error
	TTR_42602_MODE_MAX				// 20 Mode selector limit
};

// States of Tanashin mechanism for [u8_transport_mode], [u8_target_trr_mode] and [u8_user_mode].
enum
{
	TTR_TANA_MODE_TO_INIT,			// Start-up state
	TTR_TANA_MODE_INIT,				// Wait for mechanism to stabilize upon power-up
	TTR_TANA_MODE_TO_STOP,			// Start transition to STOP
	TTR_TANA_MODE_STOP,				// Stable STOP state
	TTR_TANA_MODE_PB_FWD,			// Stable PLAYBACK in forward direction
	TTR_TANA_MODE_FW_FWD,			// Stable FAST WIND in forward direction
	TTR_TANA_MODE_FW_REV,			// Stable FAST WIND in reverse direction
	TTR_TANA_MODE_TO_HALT,			// Start transition to HALT
	TTR_TANA_MODE_HALT,				// Permanent HALT due to an error
};

// Error flags of the transport for [u8_transport_error].
enum
{
	TTR_ERR_NONE = 0x00,		// No error registered
	TTR_ERR_TAPE_LOST = 0x01,	// Tape was removed during operation
	TTR_ERR_BAD_DRIVE = 0x02,	// No tacho in stop, probably bad belts or motor stall
	TTR_ERR_NO_CTRL = 0x04		// Unable to transition through modes, probably low power to solenoid, bad solenoid or actuator jammed
};

// Flags for reverse playback settings for [u8_reverse].
enum
{
	TTR_REV_ENABLE = 0x01,		// Disable all operations with reverse playback (does affect [TTR_REV_PB_AUTO] and [TTR_REV_PB_LOOP])
	TTR_REV_PB_AUTO = 0x02,		// Enable auto-reverse for forward playback (PB FWD -> PB REV -> STOP)
	TTR_REV_PB_LOOP = 0x04,		// Enable full auto-reverse (PB FWD -> PB REV -> PB FWD -> ...)
	TTR_REV_REW_AUTO = 0x08		// Enable auto-rewind for fast forward (PB FWD/FW FWD -> FW REV -> STOP) (lower priority than [TTR_REV_PB_LOOP])
};
#define TTR_REV_DEFAULT		(TTR_REV_ENABLE|TTR_REV_PB_AUTO|TTR_REV_REW_AUTO)		// Default reverse mode settings

// Index of byte in SPI bus extenders for [u8a_spi_buf].
enum
{
	SPI_IDX_IND,			// Regular transport mode indicators
	SPI_IDX_MAX				// Index limit
};

#endif /* AVRTAPE_H_ */