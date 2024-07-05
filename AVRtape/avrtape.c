#include "avrtape.h"

volatile uint8_t u8i_interrupts=0;			// Deferred interrupts call flags (non-buffered)
uint16_t u16i_last_adc_data=0;				// ADC data at last interrupt
uint8_t u8i_last_adc_mux=0;					// ADC mux settings at last interrupt
uint8_t u8i_adc_new_mux=0;					// New mux for ADC next conversion
uint8_t u8_buf_interrupts=0;				// Deferred interrupts call flags (buffered)
uint8_t u8_tasks=0;							// Deferred tasks call flags
uint8_t u8_500hz_cnt=0;						// Divider for 500 Hz
uint8_t u8_50hz_cnt=0;						// Divider for 50 Hz
uint8_t u8_10hz_cnt=0;						// Divider for 10 Hz
uint8_t u8_2hz_cnt=0;						// Divider for 2 Hz
uint8_t u8_stest_timer=0;					// Delay for self-test indication.
uint8_t u8_mech_type=TTR_TYPE_CRP42602Y;	// Selected type of mechanism
uint8_t u8_transition_timer=0;				// Solenoid holding timer
uint8_t u8_tacho_timer=0;					// Time from last tachometer signal
uint8_t u8_sleep_inh_timer=0;				// Time before next sleep is allowed
uint8_t u8_dbg_timer=0;						// Debug timer
uint8_t u8_user_mode=USR_MODE_STOP;			// User-requested mode
uint8_t u8_mech_mode=USR_MODE_STOP;			// Current user-level transport mode
uint8_t u8_last_play_dir=PB_DIR_FWD;		// Last playback direction
uint8_t u8_transport_error=TTR_ERR_NONE;	// Last transport error
uint16_t u16_features=TTR_REV_DEFAULT;		// Feature settings

uint8_t u8a_spi_buf[SPI_IDX_MAX];			// Data to send via SPI bus

#ifdef UART_TERM
char u8a_buf[48];							// Buffer for UART debug messages
#endif /* UART_TERM */

// Firmware description strings.
volatile const uint8_t ucaf_version[] PROGMEM = "v0.12";			// Firmware version
volatile const uint8_t ucaf_compile_time[] PROGMEM = __TIME__;		// Time of compilation
volatile const uint8_t ucaf_compile_date[] PROGMEM = __DATE__;		// Date of compilation
volatile const uint8_t ucaf_info[] PROGMEM = "ATmega tape transport controller";			// Firmware description
volatile const uint8_t ucaf_author[] PROGMEM = "Maksim Kryukov aka Fagear";					// Author
volatile const uint8_t ucaf_url[] PROGMEM = "https://github.com/Fagear/AVRTapeControl";		// URL

//-------------------------------------- System timer interrupt handler.
ISR(SYST_INT, ISR_NAKED)
{
	INTR_IN;
	// 1000 Hz event.
	u8i_interrupts|=INTR_SYS_TICK;
	INTR_OUT;
}

//-------------------------------------- Pin Change Interrupt Request 1.
ISR(BTN_INT, ISR_NAKED)
{
	// Used only to wakeup MCU.
	NOP;
	INTR_OUT_S;
}

//-------------------------------------- Pin Change Interrupt Request 2.
ISR(SW_INT, ISR_NAKED)
{
	// Used only to wakeup MCU.
	NOP;
	INTR_OUT_S;
}

//-------------------------------------- SPI data transmittion finished.
ISR(SPI_INT, ISR_NAKED)
{
	INTR_IN;
	u8i_interrupts|=INTR_SPI_READY;
	INTR_OUT;
}

#ifdef UART_TERM
//-------------------------------------- USART, Tx Complete.
ISR(UART_TX_INT, ISR_NAKED)
{
	INTR_IN;
	u8i_interrupts|=INTR_UART_SENT;
	INTR_OUT;
}
#endif /* UART_TERM */

//-------------------------------------- Re-configure system for fast CPU.
inline void core_prepare_on()
{
	// Disable interrupts from inputs
	BTN_DIS_INTR2; SW_DIS_INTR2;
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_sleep_out);
	UART_dump_out();
#endif /* UART_TERM */
	// Reset sleep inhibition timer.
	u8_sleep_inh_timer = 0;
	// Start system timing.
	SYST_RESET; SYST_START;
}

//-------------------------------------- Re-configure system for slow CPU.
inline void core_prepare_off()
{
	// Stop system timing.
	SYST_STOP;
	// Clear counters.
	u8i_interrupts=0;
	u8_buf_interrupts=0;
	u8_tasks=0;
	u8_500hz_cnt=0;
	u8_50hz_cnt=0;
	u8_10hz_cnt=0;
	u8_2hz_cnt=0;
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_sleep_in);
	UART_dump_out();
#endif /* UART_TERM */
	// Clear interrupt flags (write "1" to wherever "1" is).
	PCIFR = PCIFR;
	// Enable interrupts from inputs (acting as wakeup sources).
	BTN_EN_INTR2; SW_EN_INTR2;
}

//-------------------------------------- MCU sleep procedure.
static inline void CPU_power_down()
{
	core_prepare_off();
	cli();
	// Reset watchdog timer.
	wdt_reset();
	// Turn off WDT for no reset during IDLE.
	WDT_RESET_DIS;
	WDT_PREP_OFF;
	WDT_SW_OFF;
	WDT_FLUSH_REASON;
	// Enter IDLE mode.
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	sei();
	// MCU goes to sleep here.
	sleep_cpu();
	// MCU wakes up and continues here.
	sleep_disable();
	// Re-configure WDT.
	cli();
	wdt_reset();
	// Enable watchdog, prepare for prescaler change.
	WDT_PREP_ON;
	WDT_SW_ON;
	core_prepare_on();
	sei();
}

//-------------------------------------- Startup init.
inline void system_startup(void)
{
	// Shut down watchdog.
	// Prevent mis-configured watchdog loop reset.
	wdt_reset();
	cli();
	WDT_RESET_DIS;
	WDT_PREP_OFF;
	WDT_SW_OFF;
	WDT_FLUSH_REASON;

	// Init hardware resources.
	HW_init();

	// Start system timer.
	SYST_START;

	// Enable watchdog (reset in ~4 s).
	WDT_PREP_ON;
	WDT_SW_ON;
	wdt_reset();
}

//-------------------------------------- Slow events dividers.
inline void slow_timing(void)
{
	u8_500hz_cnt++;
	if(u8_500hz_cnt>=2)		// 1000/2 = 500
	{
		u8_500hz_cnt = 0;
		// 500 Hz event.
		u8_tasks |= TASK_500HZ;

		u8_50hz_cnt++;
		if(u8_50hz_cnt>=10)		// 500/10 = 50.
		{
			u8_50hz_cnt = 0;
			// 50 Hz event.
			u8_tasks |= TASK_50HZ;

			u8_10hz_cnt++;
			if(u8_10hz_cnt>=5)	// 50/5 = 10.
			{
				u8_10hz_cnt = 0;
				// 10 Hz event.
				u8_tasks |= TASK_10HZ;

				u8_2hz_cnt++;
				if(u8_2hz_cnt>=5)	// 10/5 = 2.
				{
					u8_2hz_cnt = 0;
					// 2 Hz event.
					u8_tasks |= TASK_2HZ;
				}
			}
		}
	}
}

uint8_t sw_state = 0;
uint8_t sw_pressed = 0;
uint8_t sw_released = 0;
//-------------------------------------- Scan sensors of the transport.
inline void switches_scan(void)
{
	// Check TAPE_IN sensor.
	if(SW_TAPE_IN_STATE==0)
	{
		// Switch is active.
		// Compare with previous state.
		if((sw_state&TTR_SW_TAPE_IN)==0)
		{
			// Switch was not active before, now it is.
			sw_state|=TTR_SW_TAPE_IN;
			sw_pressed|=TTR_SW_TAPE_IN;
		}
	}
	else
	{
		// Switch is inactive.
		// Compare with previous state.
		if((sw_state&TTR_SW_TAPE_IN)!=0)
		{
			// Switch was active, now it is not.
			sw_state&=~TTR_SW_TAPE_IN;
			sw_released|=TTR_SW_TAPE_IN;
		}
	}
	// Check STOP sensor.
	if(SW_STOP_STATE!=0)
	{
		if((sw_state&TTR_SW_STOP)==0)
		{
			sw_state|=TTR_SW_STOP;
			sw_pressed|=TTR_SW_STOP;
		}
	}
	else
	{
		if((sw_state&TTR_SW_STOP)!=0)
		{
			sw_state&=~TTR_SW_STOP;
			sw_released|=TTR_SW_STOP;
		}
	}
	// Check REC_INHIBIT sensor for forward direction.
	if(SW_NOREC_FWD_STATE!=0)
	{
		if((sw_state&TTR_SW_NOREC_FWD)==0)
		{
			sw_state|=TTR_SW_NOREC_FWD;
			sw_pressed|=TTR_SW_NOREC_FWD;
		}
	}
	else
	{
		if((sw_state&TTR_SW_NOREC_FWD)!=0)
		{
			sw_state&=~TTR_SW_NOREC_FWD;
			sw_released|=TTR_SW_NOREC_FWD;
		}
	}
	// Check REC_INHIBIT sensor for reverse direction.
	if(SW_NOREC_REV_STATE!=0)
	{
		if((sw_state&TTR_SW_NOREC_REV)==0)
		{
			sw_state|=TTR_SW_NOREC_REV;
			sw_pressed|=TTR_SW_NOREC_REV;
		}
	}
	else
	{
		if((sw_state&TTR_SW_NOREC_REV)!=0)
		{
			sw_state&=~TTR_SW_NOREC_REV;
			sw_released|=TTR_SW_NOREC_REV;
		}
	}
	if(((sw_pressed&(TTR_SW_TAPE_IN|TTR_SW_NOREC_FWD|TTR_SW_NOREC_REV))!=0)||
		((sw_released&(TTR_SW_TAPE_IN|TTR_SW_NOREC_FWD|TTR_SW_NOREC_REV))!=0))
	{
		// Reset sleep inhibition timer.
		u8_sleep_inh_timer = 0;
#ifdef UART_TERM
		sprintf(u8a_buf, "SWS|TAPE:%01d|F_REC:%01d|R_REC:%01d\n\r",
				((sw_state&TTR_SW_TAPE_IN)==0)?0:1,
				((sw_state&TTR_SW_NOREC_FWD)==0)?0:1,
				((sw_state&TTR_SW_NOREC_REV)==0)?0:1);
		UART_add_string(u8a_buf);
#endif /* UART_TERM */
	}
}

uint8_t kbd_state = 0;			// Buttons states from the last [keys_simple_scan()] poll.
uint8_t kbd_pressed = 0;		// Flags for buttons that have been pressed (should be cleared after processing).
uint8_t kbd_released = 0;		// Flags for buttons that have been released (should be cleared after processing).
//-------------------------------------- Keyboard scan routine.
inline void keys_simple_scan(void)
{
	// Button "STOP".
	// Check button input.
	if(BTN_STOP_STATE==0)
	{
		// Button is pressed.
		// Compare with previous state.
		if((kbd_state&USR_BTN_STOP)==0)
		{
			// Button was released before, now it is pressed.
			kbd_state|=USR_BTN_STOP;
			kbd_pressed|=USR_BTN_STOP;
		}
	}
	else
	{
		// Button is released.
		// Compare with previous state.
		if((kbd_state&USR_BTN_STOP)!=0)
		{
			// Button was pressed before, now it is released.
			kbd_state&=~USR_BTN_STOP;
			kbd_released|=USR_BTN_STOP;
		}
	}
	// Button "PLAY".
	if(BTN_PLAY_STATE==0)
	{
		if((kbd_state&USR_BTN_PLAY)==0)
		{
			kbd_state|=USR_BTN_PLAY;
			kbd_pressed|=USR_BTN_PLAY;
		}
	}
	else
	{
		if((kbd_state&USR_BTN_PLAY)!=0)
		{
			kbd_state&=~USR_BTN_PLAY;
			kbd_released|=USR_BTN_PLAY;
		}
	}
	// Button "PLAY IN REVERSE".
	if(BTN_PLAY_REV_STATE==0)
	{
		if((kbd_state&USR_BTN_PLAY_REV)==0)
		{
			kbd_state|=USR_BTN_PLAY_REV;
			kbd_pressed|=USR_BTN_PLAY_REV;
		}
	}
	else
	{
		if((kbd_state&USR_BTN_PLAY_REV)!=0)
		{
			kbd_state&=~USR_BTN_PLAY_REV;
			kbd_released|=USR_BTN_PLAY_REV;
		}
	}
	// Button "FAST WIND FORWARD".
	if(BTN_FFWD_STATE==0)
	{
		if((kbd_state&USR_BTN_FFORWARD)==0)
		{
			kbd_state|=USR_BTN_FFORWARD;
			kbd_pressed|=USR_BTN_FFORWARD;
		}
	}
	else
	{
		if((kbd_state&USR_BTN_FFORWARD)!=0)
		{
			kbd_state&=~USR_BTN_FFORWARD;
			kbd_released|=USR_BTN_FFORWARD;
		}
	}
	// Button "FAST WIND IN REVERSE".
	if(BTN_REWD_STATE==0)
	{
		if((kbd_state&USR_BTN_REWIND)==0)
		{
			kbd_state|=USR_BTN_REWIND;
			kbd_pressed|=USR_BTN_REWIND;
		}
	}
	else
	{
		if((kbd_state&USR_BTN_REWIND)!=0)
		{
			kbd_state&=~USR_BTN_REWIND;
			kbd_released|=USR_BTN_REWIND;
		}
	}
	// Button "RECORD".
	if(BTN_REC_STATE==0)
	{
		if((kbd_state&USR_BTN_RECORD)==0)
		{
			kbd_state|=USR_BTN_RECORD;
			kbd_pressed|=USR_BTN_RECORD;
		}
	}
	else
	{
		if((kbd_state&USR_BTN_RECORD)!=0)
		{
			kbd_state&=~USR_BTN_RECORD;
			kbd_released|=USR_BTN_RECORD;
		}
	}
	if(kbd_pressed!=0)
	{
		// Reset sleep inhibition timer.
		u8_sleep_inh_timer = 0;
#ifdef UART_TERM
		sprintf(u8a_buf, "KBD|REWN:%01d|STOP:%01d|FFWD:%01d|PB_F:%01d|PB_R:%01d|REC:%01d\n\r",
				((kbd_pressed&USR_BTN_REWIND)==0)?0:1,
				((kbd_pressed&USR_BTN_STOP)==0)?0:1,
				((kbd_pressed&USR_BTN_FFORWARD)==0)?0:1,
				((kbd_pressed&USR_BTN_PLAY)==0)?0:1,
				((kbd_pressed&USR_BTN_PLAY_REV)==0)?0:1,
				((kbd_pressed&USR_BTN_RECORD)==0)?0:1);
		UART_add_string(u8a_buf);
#endif /* UART_TERM */
	}
}

//-------------------------------------- Start-up test for number of connected playback buttons.
void scan_pb_buttons(void)
{
	// Set "single playback button" state.
	u16_features &= ~TTR_FEA_TWO_PLAYS;
	// Let pin stabilize.
	_delay_us(10);
	// Pull both buttons up as inputs.
	BTN_DIR_1 &= ~(BTN_PLAY|BTN_PLAY_REV);
	BTN_PORT_1 |= (BTN_PLAY|BTN_PLAY_REV);
	// Let pin stabilize.
	_delay_us(20);
	// Check if both inputs are held at "1".
	if(BTN_PLAY_STATE==0)
	{
		// PLAY is at "0".
		if(BTN_PLAY_REV_STATE!=0)
		{
			// REVERSE PLAY is at "1".
			// If buttons are in different state that implies that there are two separate inputs.
			u16_features |= TTR_FEA_TWO_PLAYS;
		}
		/*else
		{
			// REVERSE PLAY is at "0".
			// If both buttons are at "0" that can imply a combined input but it also can indicate two buttons held down.
			// Can not determine further, quit.
		}*/
	}
	else
	{
		// PLAY is at "1".
		if(BTN_PLAY_REV_STATE==0)
		{
			// REVERSE PLAY is at "0".
			// If buttons are in different state that implies that there are two separate inputs.
			u16_features |= TTR_FEA_TWO_PLAYS;
		}
		else
		{
			// REVERSE PLAY is also at "1".
			// Pull PLAY pin low, hard.
			BTN_PORT_1 &= ~BTN_PLAY;
			BTN_DIR_1 |= BTN_PLAY;
			// Let pin stabilize.
			_delay_us(10);
			// Check if REVERSE PLAY input has switched to "0".
			if(BTN_PLAY_REV_STATE!=0)
			{
				// REVERSE PLAY stayed at "1".
				// REVERSE PLAY as input doesn't follow state of PLAY pin set as output,
				// implying that those pins are separate, requesting double play button operation.
				u16_features |= TTR_FEA_TWO_PLAYS;
			}
		}
	}
	// Set both pins to perform as pulled-up inputs.
	BTN_DIR_1 &= ~(BTN_PLAY|BTN_PLAY_REV);
	BTN_PORT_1 |= (BTN_PLAY|BTN_PLAY_REV);
	// Let pins stabilize.
	_delay_us(20);
}

//-------------------------------------- Start-up check for held down STOP button.
void scan_selftest_buttons(void)
{
	// Test button several times.
	for(uint8_t idx=0;idx<50;idx++)
	{
		_delay_us(10);
		if(BTN_STOP_STATE!=0)
		{
			// STOP button is not pressed.
			// Cancel self-test.
			u8_tasks &= ~TASK_SCAN_STEST;
			return;
		}
	}
}

//-------------------------------------- Poll tachometer transitions.
inline void poll_tacho(void)
{
	// Check takeup tachometer sensor.
	if(SW_TACHO_STATE==0)
	{
		if((sw_state&TTR_SW_TACHO)==0)
		{
			sw_state|=TTR_SW_TACHO;
			sw_pressed|=TTR_SW_TACHO;
		}
	}
	else
	{
		if((sw_state&TTR_SW_TACHO)!=0)
		{
			sw_state&=~TTR_SW_TACHO;
			sw_released|=TTR_SW_TACHO;
		}
	}
	
	if(((sw_pressed&TTR_SW_TACHO)!=0)||((sw_released&TTR_SW_TACHO)!=0))
	{
		// Tachometer has changed its state from last time.
		sw_pressed&=~TTR_SW_TACHO;
		sw_released&=~TTR_SW_TACHO;
		// Reset tacho timer.
		u8_tacho_timer = 0;
	}
}

//-------------------------------------- Count up tachometer timer.
inline void count_up_tacho(void)
{
	if(u8_tacho_timer<240)
	{
		// Count delay up if possible.
		u8_tacho_timer++;
	}
}

//-------------------------------------- Update indication.
inline void update_indicators(void)
{
	// Tape presence indicator.
	if((sw_state&TTR_SW_TAPE_IN)!=0)
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_TAPE;
	}
	else
	{
		u8a_spi_buf[SPI_IDX_IND] &= ~IND_TAPE;
	}
	// Check if transport is in an error.
	if(u8_transport_error==TTR_ERR_NONE)
	{
		// No transport error, normal operation.
		// Clear error indicator.
		u8a_spi_buf[SPI_IDX_IND] &= ~IND_ERROR;
		if(u8_mech_mode==USR_MODE_STOP)
		{
			// User-desirable mode: STOP.
			// Check if transport reached target mode.
			if(u8_transition_timer==0)
			{
				// Mode is reached, light up STOP indicator.
				u8a_spi_buf[SPI_IDX_IND] |= IND_STOP;
			}
			// Mode transition in progress: make a fast blink.
			else if((u8_tasks&TASK_FAST_BLINK)!=0)
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_STOP;
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~IND_STOP;
			}
		}
		else
		{
			// Clear STOP indicator.
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_STOP;
		}
		// Playback forward indicator/playback indicator.
		if((u16_features&TTR_FEA_TWO_PLAYS)==0)
		{
			// Single playback button (clicking toggles direction) and Playback LED.
			// Playback indicator.
			if((u8_mech_mode==USR_MODE_PLAY_FWD)||
				(u8_mech_mode==USR_MODE_PLAY_REV||
				(u8_mech_mode==USR_MODE_REC_FWD)||
				(u8_mech_mode==USR_MODE_REC_REV)))
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY;
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY;
			}
			// Playback direction indicator.
			//if((u8_mech_mode==USR_MODE_PLAY_REV)||(u8_mech_mode==USR_MODE_REC_REV))
			if(u8_last_play_dir!=PB_DIR_FWD)
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_DIR;
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY_DIR;
			}
		}
		else
		{
			// Two playback buttons (for each direction) and two Playback LEDs.
			// Playback forward indicator.
			if((u8_mech_mode==USR_MODE_PLAY_FWD)||(u8_mech_mode==USR_MODE_REC_FWD))
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_FWD;
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY_FWD;
			}
			// Playback backwards indicator.
			if((u8_mech_mode==USR_MODE_PLAY_REV)||(u8_mech_mode==USR_MODE_REC_REV))
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_REV;
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY_REV;
			}
		}
		// Record indicator.
		if((u8_mech_mode==USR_MODE_REC_FWD)||(u8_mech_mode==USR_MODE_REC_REV))
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_REC;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_REC;
		}
		// Fast forward indicator.
		if(u8_mech_mode==USR_MODE_FWIND_FWD)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_FFORWARD;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_FFORWARD;
		}
		// Rewind indicator.
		if(u8_mech_mode==USR_MODE_FWIND_REV)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_REWIND;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_REWIND;
		}
	}
	else
	{
		// Transport error detected.
		// Clear every other mode indicators.
		u8a_spi_buf[SPI_IDX_IND] &= ~(IND_STOP|IND_PLAY_FWD|IND_PLAY_REV|IND_REC|IND_FFORWARD|IND_REWIND);
		if((u8_transport_error&TTR_ERR_BAD_DRIVE)!=0)
		{
			if((u8_tasks&TASK_SLOW_BLINK)!=0)
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_ERROR;
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~IND_ERROR;
			}
		}
		else if((u8_transport_error&TTR_ERR_NO_CTRL)!=0)
		{
			if((u8_tasks&TASK_FAST_BLINK)!=0)
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_ERROR;
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~IND_ERROR;
			}
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_ERROR;
		}
	}
	// Transmit indicator information via SPI.
	SPI_TX_START;
	SPI_send_byte(u8a_spi_buf[SPI_IDX_IND]);
}

//-------------------------------------- Self-test mode indication.
inline void selftest_indicators(void)
{
	u8a_spi_buf[SPI_IDX_IND] &= ~(IND_TAPE|IND_REC);
	u8a_spi_buf[SPI_IDX_IND] |= IND_ERROR;
	// Tape presence indicator.
	if((sw_state&TTR_SW_TAPE_IN)!=0)
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_TAPE;
	}
	// Record inhibit switches.
	if((sw_state&TTR_SW_NOREC_FWD)==0)
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_REC;
	}
	if((u16_features&TTR_FEA_REV_ENABLE)!=0)
	{
		if((sw_state&TTR_SW_NOREC_REV)==0)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_REC;
		}
	}
	// Switch mode lights once a second.
	if((u8_stest_timer==90)||(u8_stest_timer==60)||(u8_stest_timer==30))
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_STOP;
		u8a_spi_buf[SPI_IDX_IND] &= ~(IND_PLAY_REV|IND_PLAY|IND_REWIND|IND_FFORWARD);
	}
	else if((u8_stest_timer==80)||(u8_stest_timer==50)||(u8_stest_timer==20))
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_REV|IND_PLAY;
		u8a_spi_buf[SPI_IDX_IND] &= ~(IND_STOP|IND_REWIND|IND_FFORWARD);
	}
	else if((u8_stest_timer==70)||(u8_stest_timer==40)||(u8_stest_timer==10))
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_REWIND|IND_FFORWARD;
		u8a_spi_buf[SPI_IDX_IND] &= ~(IND_STOP|IND_PLAY_REV|IND_PLAY);
	}
	// Count down self-test mode.
	if(u8_stest_timer!=0) u8_stest_timer--;
	if(u8_stest_timer==0)
	{
		// Clear self-test mode.
		u8_tasks &= ~TASK_SCAN_STEST;
		// Clear indicators.
		u8a_spi_buf[SPI_IDX_IND] &= (uint8_t)~(IND_ERROR|IND_TAPE|IND_STOP|IND_PLAY_REV|IND_PLAY|IND_REWIND|IND_FFORWARD|IND_REC);
	}
	// Reset sleep timer.
	u8_sleep_inh_timer = 0;
	// Transmit indicator information via SPI.
	SPI_TX_START;
	SPI_send_byte(u8a_spi_buf[SPI_IDX_IND]);
}

//-------------------------------------- Process input from user.
void process_user(void)
{
#ifdef UART_TERM
	uint8_t last_user_mode;
	// Save mode.
	last_user_mode = u8_user_mode;
#endif /* UART_TERM */
	if(((kbd_state&USR_BTN_RECORD)!=0)&&(u8_user_mode!=USR_MODE_STOP))
	{
		// Ignore all RECORD+ combinations if not in STOP.
		kbd_pressed&=~(USR_BTN_REWIND|USR_BTN_PLAY_REV|USR_BTN_STOP|USR_BTN_PLAY|USR_BTN_FFORWARD);
	}
	// Always clear RECORD events.
	kbd_pressed&=~USR_BTN_RECORD;
	// Button priority: from lowest to highest.
	if((kbd_pressed&USR_BTN_REWIND)!=0)
	{
		kbd_pressed&=~USR_BTN_REWIND;
		// Rewind.
		u8_user_mode = USR_MODE_FWIND_REV;
	}
	if((kbd_pressed&USR_BTN_FFORWARD)!=0)
	{
		kbd_pressed&=~USR_BTN_FFORWARD;
		// Fast forward.
		u8_user_mode = USR_MODE_FWIND_FWD;
	}
	// Check reverse settings.
	if((u16_features&TTR_FEA_REV_ENABLE)==0)
	{
		// Reverse operations are disabled.
		if((kbd_pressed&USR_BTN_PLAY)!=0)
		{
			kbd_pressed&=~USR_BTN_PLAY;
			// Check if RECORD button is held.
			if(((kbd_state&USR_BTN_RECORD)==0)||((sw_state&TTR_SW_NOREC_FWD)!=0))
			{
				// Record button is not held or REC INHIBIT in forward direction is active.
				// Start/resume playback in forward direction.
				u8_user_mode = USR_MODE_PLAY_FWD;
			}
			else
			{
				// Start/resume recording in forward direction.
				u8_user_mode = USR_MODE_REC_FWD;
			}
		}
		// Always clear second playback button flag.
		kbd_pressed&=~USR_BTN_PLAY_REV;
	}
	else
	{
		// Check how many playback buttons are available.
		if((u16_features&TTR_FEA_TWO_PLAYS)==0)
		{
			// Single playback button (clicking toggles direction).
			if((kbd_pressed&USR_BTN_PLAY)!=0)
			{
				kbd_pressed&=~USR_BTN_PLAY;
				// Check if current mode is playback already and check current direction.
				if(u8_user_mode==USR_MODE_PLAY_FWD)
				{
					// Swap tape side.
					u8_user_mode = USR_MODE_PLAY_REV;
				}
				else if(u8_user_mode==USR_MODE_PLAY_REV)
				{
					// Swap tape side.
					u8_user_mode = USR_MODE_PLAY_FWD;
				}
				else
				{
					// Current mode is not PLAY.
					if((kbd_state&USR_BTN_RECORD)==0)
					{
						// Record is not requested or TTR is not in STOP.
						// Check last playback direction.
						if(u8_last_play_dir==PB_DIR_FWD)
						{
							// Start/resume playback in forward direction.
							u8_user_mode = USR_MODE_PLAY_FWD;
						}
						else
						{
							// Start/resume playback in reverse direction.
							u8_user_mode = USR_MODE_PLAY_REV;
						}
					}
					else
					{
						// Record is requested and TTR is in STOP.
						// Check last playback direction.
						if(u8_last_play_dir==PB_DIR_FWD)
						{
							// Last direction: forward.
							// Check record inhibit switch for forward direction.
							if((sw_state&TTR_SW_NOREC_FWD)==0)
							{
								// Recording is allowed.
								// Start/resume recording in forward direction.
								u8_user_mode = USR_MODE_REC_FWD;
							}
							else
							{
								// Record is inhibited.
								// Start/resume playback in forward direction.
								u8_user_mode = USR_MODE_PLAY_FWD;
							}
						}
						else
						{
							// Last direction: reverse.
							// Check record inhibit switch for reverse direction.
							if((sw_state&TTR_SW_NOREC_REV)==0)
							{
								// Recording is allowed.
								// Start/resume recording in reverse direction.
								u8_user_mode = USR_MODE_REC_REV;
							}
							else
							{
								// Record is inhibited.
								// Start/resume playback in reverse direction.
								u8_user_mode = USR_MODE_PLAY_REV;
							}
						}
					}
				}
			}
			// Always clear second playback button flag.
			kbd_pressed&=~USR_BTN_PLAY_REV;
		}
		else
		{
			// Two playback buttons (for each direction).
			if((kbd_state&USR_BTN_RECORD)==0)
			{
				// Record is not requested or TTR is not in STOP.
				if((kbd_pressed&USR_BTN_PLAY_REV)!=0)
				{
					kbd_pressed&=~USR_BTN_PLAY_REV;
					// Start/resume playback in reverse direction (less priority).
					u8_user_mode = USR_MODE_PLAY_REV;
				}
				if((kbd_pressed&USR_BTN_PLAY)!=0)
				{
					kbd_pressed&=~USR_BTN_PLAY;
					// Start/resume playback in forward direction (more priority).
					u8_user_mode = USR_MODE_PLAY_FWD;
				}
			}
			else
			{
				// Record is requested.
				if((kbd_pressed&USR_BTN_PLAY_REV)!=0)
				{
					kbd_pressed&=~USR_BTN_PLAY_REV;
					// Check record inhibit switch for reverse direction.
					if((sw_state&TTR_SW_NOREC_REV)==0)
					{
						// Recording is allowed.
						// Start/resume recording in reverse direction (less priority).
						u8_user_mode = USR_MODE_REC_REV;
					}
					else
					{
						// Record is inhibited.
						// Start/resume playback in reverse direction (less priority).
						u8_user_mode = USR_MODE_PLAY_REV;
					}
				}
				if((kbd_pressed&USR_BTN_PLAY)!=0)
				{
					kbd_pressed&=~USR_BTN_PLAY;
					// Check record inhibit switch for forward direction.
					if((sw_state&TTR_SW_NOREC_FWD)==0)
					{
						// Recording is allowed.
						// Start/resume recording in forward direction (more priority).
						u8_user_mode = USR_MODE_REC_FWD;
					}
					else
					{
						// Record is inhibited.
						// Start/resume playback in forward direction (more priority).
						u8_user_mode = USR_MODE_PLAY_FWD;
					}
				}
			}
		}
	}
	if((kbd_pressed&USR_BTN_STOP)!=0)
	{
		kbd_pressed&=~USR_BTN_STOP;
		// Stop the tape.
		u8_user_mode = USR_MODE_STOP;
	}
#ifdef UART_TERM
	if(last_user_mode!=u8_user_mode)
	{
		// Log user mode change.
		UART_add_flash_string((uint8_t *)cch_new_user_mode);
		UART_dump_user_mode(last_user_mode); UART_add_flash_string((uint8_t *)cch_arrow); UART_dump_user_mode(u8_user_mode);
		UART_add_flash_string((uint8_t *)cch_endl);
	}
#endif /* UART_TERM */
}

void mech_log()
{
	if(u8_user_mode!=USR_MODE_STOP)
	{
		/*if(u8_user_mode==USR_MODE_FWIND_REV)
		{
			if(u8_dbg_timer>10)
			{
				u8_dbg_timer -= 5;
				sprintf(u8a_buf, "REP- @|%03u\n\r", u8_dbg_timer);
				UART_add_string(u8a_buf);
			}
			u8_user_mode = u8_transport_mode;
		}
		if(u8_user_mode==USR_MODE_FWIND_FWD)
		{
			if(u8_dbg_timer<240)
			{
				u8_dbg_timer += 5;
				sprintf(u8a_buf, "REP+ @|%03u\n\r", u8_dbg_timer);
				UART_add_string(u8a_buf);
			}
			u8_user_mode = u8_transport_mode;
		}
		if(u8_user_mode==USR_MODE_PLAY_FWD)
		{
			// Preset time for full playback selection cycle from STOP.
			//u8_transition_timer = TIM_TANASHIN_DLY_PB_WAIT;
			u8_transition_timer = 255;
			SOLENOID_ON;
		}*/
		if(u8_user_mode==USR_MODE_PLAY_FWD)
		{
			// Preset time for full cycle from STOP to PLAY.
			//u8_transition_timer = TIM_TANA_DLY_PB_WAIT;
		}
		else if(u8_user_mode==USR_MODE_FWIND_REV)
		{
			// Preset time for full cycle from PLAY to FAST WIND.
			//u8_transition_timer = TIM_TANA_DLY_FWIND_WAIT;
		}
		else if(u8_user_mode==USR_MODE_FWIND_FWD)
		{
			// Preset time for full cycle from PLAY to FAST WIND.
			//u8_transition_timer = TIM_TANA_DLY_FWIND_WAIT;
		}
		u8_user_mode = USR_MODE_STOP;
	}

	if(u8_transition_timer>0)
	{
#ifdef UART_TERM
		uint8_t sol_state;
		sol_state = 0;
		if(SOLENOID_STATE!=0) sol_state = 1;
		sprintf(u8a_buf, "MODE|>%03u<|%02x|%01x\n\r", u8_transition_timer, sw_state, sol_state);
		UART_add_string(u8a_buf);
#endif /* UART_TERM */

		/*if(u8_user_mode==USR_MODE_PLAY_FWD)
		{
			if(u8_transition_timer>(TIM_TANA_DLY_PB_WAIT-TIM_TANA_DLY_SW_ACT))
			{
				// Enable solenoid to start gear rotation.
				SOLENOID_ON;
			}
			else
			{
				// Disable solenoid for the reset of the PLAY selection cycle.
				SOLENOID_OFF;
			}
		}
		else if((u8_user_mode==USR_MODE_FWIND_FWD)||(u8_user_mode==USR_MODE_FWIND_REV))
		{
			if(u8_transition_timer>(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_SW_ACT))
			{
				// Enable solenoid to start gear rotation.
				SOLENOID_ON;
			}
			else if(u8_transition_timer>(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_WAIT_REW_ACT))
			{
				// Disable solenoid, wait for the decision making point for fast wind direction.
				SOLENOID_OFF;
			}
			else if((u8_transition_timer>(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_WAIT_REW_ACT-TIM_TANA_DLY_SW_ACT))&&(u8_user_mode==USR_MODE_FWIND_REV))
			{
				// Enable solenoid for the rewind.
				SOLENOID_ON;
			}
			else
			{
				// Disable solenoid for the reset of the FAST WIND selection cycle.
				SOLENOID_OFF;
			}
		}*/
		u8_transition_timer--;
		/*if(u8_transition_timer<(u8_dbg_timer-TIM_TANASHIN_DLY_SW_ACT))
		{
			SOLENOID_OFF;
		}
		else if(u8_transition_timer<u8_dbg_timer)
		{
			SOLENOID_ON;
		}
		else if(u8_transition_timer<(255-TIM_TANASHIN_DLY_SW_ACT))
		{
			SOLENOID_OFF;
		}*/
		if(u8_transition_timer==0)
		{
			SOLENOID_OFF;
			//u8_transport_mode = TTR_42602_MODE_STOP;
			u8_user_mode = USR_MODE_STOP;
		}

	}
#ifdef UART_TERM
	UART_dump_out();
#endif /* UART_TERM */
}

//-------------------------------------- Main function.
int main(void)
{
	// Start-up initialization.
	system_startup();

	// Start SPI comms.
	SPI_int_enable();
	SPI_TX_START;
	SPI_send_byte(0x00);

	// Preload startup tests.
	u8_tasks |= (TASK_SCAN_PB_BTNS|TASK_SCAN_STEST);
			
	// Default mech.
#ifdef SUPP_CRP42602Y_MECH
	u8_mech_type = TTR_TYPE_CRP42602Y;
#endif /* SUPP_CRP42602Y_MECH */
#ifdef SUPP_TANASHIN_MECH
	u8_mech_type = TTR_TYPE_TANASHIN;
	u16_features &= ~(TTR_FEA_STOP_TACHO|TTR_FEA_REV_ENABLE|TTR_FEA_PB_AUTOREV|TTR_FEA_PB_LOOP|TTR_FEA_TWO_PLAYS);	// Disable functions for non-reverse mech.
	// This transport does not have reverse record inhibit switch,
	// using this pin as power supply for tacho sensor.
	TANA_TACHO_PWR_SETUP;
	TANA_TACHO_PWR_EN;
#endif /* SUPP_TANASHIN_MECH */

	// Init modes to selected transport.
	u8_user_mode = USR_MODE_STOP;
#ifdef UART_TERM
	// Output startup messages.
	UART_add_flash_string((uint8_t *)cch_startup_1);
	UART_add_flash_string((uint8_t *)ucaf_author); UART_add_flash_string((uint8_t *)cch_endl); UART_dump_out();
	UART_add_flash_string((uint8_t *)ucaf_info); UART_add_flash_string((uint8_t *)cch_endl);
	UART_add_flash_string((uint8_t *)ucaf_version); UART_add_string(" ["); UART_add_flash_string((uint8_t *)ucaf_compile_date); UART_add_string(", "); UART_add_flash_string((uint8_t *)ucaf_compile_time); UART_add_string("]"); UART_add_flash_string((uint8_t *)cch_endl);
#ifdef SUPP_CRP42602Y_MECH
	if(u8_mech_type==TTR_TYPE_CRP42602Y)
	{
		UART_add_flash_string((uint8_t *)cch_tape_transport); UART_add_flash_string((uint8_t *)ucaf_crp42602y_mech); UART_add_flash_string((uint8_t *)cch_endl);
	}
#endif /* SUPP_CRP42602Y_MECH */
#ifdef SUPP_TANASHIN_MECH
	if(u8_mech_type==TTR_TYPE_TANASHIN)
	{
		UART_add_flash_string((uint8_t *)cch_tape_transport); UART_add_flash_string((uint8_t *)ucaf_tanashin_mech); UART_add_flash_string((uint8_t *)cch_endl);
	}
#endif /* SUPP_TANASHIN_MECH */
	UART_add_flash_string((uint8_t *)cch_endl); UART_dump_settings(u16_features); UART_add_flash_string((uint8_t *)cch_endl);
	UART_dump_out();
#endif /* UART_TERM */

	if((u8_tasks&TASK_SCAN_PB_BTNS)!=0)
	{
		// Check for short between two playback button inputs
		// to determine how many playback buttons is required.
		scan_pb_buttons();
	}
	
	// Check if STOP button is held at start-up
	// to enable self-test mode.
	scan_selftest_buttons();
	if((u8_tasks&TASK_SCAN_STEST)!=0)
	{
		u8_stest_timer = 100;
	}

	// Enable interrupts globally.
	sei();

    // Main cycle.
    while(1)
    {
		// Disable interrupts globally.
	    cli();
	    // Buffer all interrupts.
	    u8_buf_interrupts|=u8i_interrupts;
	    // Clear all interrupt flags.
	    u8i_interrupts=0;
		// Enable interrupts globally.
	    sei();

	    // Process deferred tasks.
		if((u8_buf_interrupts&INTR_SYS_TICK)!=0)
		{
			u8_buf_interrupts&=~INTR_SYS_TICK;
			// System timing: 1000 Hz, 1000 us period.
			// Process additional slow timers.
			slow_timing();

			// Process slow events.
			if((u8_tasks&TASK_2HZ)!=0)
			{
				u8_tasks&=~TASK_2HZ;
				// 2 Hz event, 500 ms period.
				// Toggle slow blink flag.
				u8_tasks^=TASK_SLOW_BLINK;
				// Reset watchdog timer.
				wdt_reset();
				// Increase sleep inhibition timer.
				if(u8_sleep_inh_timer<SLEEP_INHIBIT_2HZ)
				{
					u8_sleep_inh_timer++;
				}
#ifdef UART_TERM
				//sprintf(u8a_buf, "SLEEP|%05u|%03u\n\r", u16_crp42602y_idle_time, u8_tacho_timer);
				//UART_add_string(u8a_buf);
#endif /* UART_TERM */
			}
			if((u8_tasks&TASK_10HZ)!=0)
			{
				u8_tasks&=~TASK_10HZ;
				// 10 Hz event, 100 ms period.
				// Toggle fast blink flag.
				u8_tasks^=TASK_FAST_BLINK;
				if((u8_tasks&TASK_SCAN_STEST)!=0)
				{
					// Self-test indication.
					selftest_indicators();
				}
#ifdef UART_TERM
				//sprintf(u8a_buf, "SLEEP|%02u|%05u|%03u\n\r", u8_crp42602y_mode, u16_crp42602y_idle_time, u8_tacho_timer);
				//UART_add_string(u8a_buf);
#endif /* UART_TERM */
			}
			if((u8_tasks&TASK_50HZ)!=0)
			{
				// ~105 us @ 1 MHz (without UART logging)
				u8_tasks&=~TASK_50HZ;
				// 50 Hz event, 20 ms period.
				// Scan user keys.
				keys_simple_scan();
				// Scan switches and sensors.
				switches_scan();
				// Increase tachometer timer.
				count_up_tacho();
				if((u8_tasks&TASK_SCAN_STEST)==0)
				{
					// Process user input.
					process_user();
					// Update LEDs.
					update_indicators();
				}
#ifdef UART_TERM
				if(UART_get_sending_number()>0)
				{
					u8_buf_interrupts|=INTR_UART_SENT;
				}
#endif /* UART_TERM */
			}
			if((u8_tasks&TASK_500HZ)!=0)
			{
				// ~265 us @ 1 MHz (without UART logging)
				u8_tasks&=~TASK_500HZ;
				// 500 Hz event, 2 ms period.
				// Scan tachometer.
				poll_tacho();
				// Check if transport operation is allowed.
				if((u8_tasks&TASK_SCAN_STEST)==0)
				{
					// Update transport state machine and solenoid action.
					if(u8_mech_type==TTR_TYPE_CRP42602Y)
					{
#ifdef SUPP_CRP42602Y_MECH
						// Processing for CRP42602Y mechanism.
						mech_crp42602y_state_machine(u16_features, sw_state, &u8_tacho_timer, &u8_user_mode, &u8_last_play_dir);
						u8_mech_mode = mech_crp42602y_get_mode();
						u8_transition_timer = mech_crp42602y_get_transition();
						u8_transport_error = mech_crp42602y_get_error();
#endif /* SUPP_CRP42602Y_MECH */
					}
					else if(u8_mech_type==TTR_TYPE_TANASHIN)
					{
#ifdef SUPP_TANASHIN_MECH
						// Processing for Tanashin-clone mechanism.
						mech_tanashin_state_machine(u16_features, sw_state, &u8_tacho_timer, &u8_user_mode, &u8_last_play_dir);
						u8_mech_mode = mech_tanashin_get_mode();
						u8_transition_timer = mech_tanashin_get_transition();
						u8_transport_error = mech_tanashin_get_error();
#endif /* SUPP_TANASHIN_MECH */
					}
					else
					{
						mech_log();
					}
				}
#ifdef UART_TERM
				uint8_t u8_old_dir;
				// Log tape direction change.
				u8_old_dir = u8_last_play_dir;
				if(u8_old_dir!=u8_last_play_dir)
				{
					if(u8_last_play_dir==PB_DIR_FWD)
					{
						UART_add_flash_string((uint8_t *)cch_pb_dir); UART_add_flash_string((uint8_t *)cch_forward);
					}
					else
					{
						UART_add_flash_string((uint8_t *)cch_pb_dir); UART_add_flash_string((uint8_t *)cch_reverse);
					}
				}
#endif /* UART_TERM */
				// Clear switches events.
				sw_pressed&=~(TTR_SW_TAPE_IN|TTR_SW_STOP|TTR_SW_NOREC_FWD|TTR_SW_NOREC_REV);
				sw_released&=~(TTR_SW_TAPE_IN|TTR_SW_STOP|TTR_SW_NOREC_FWD|TTR_SW_NOREC_REV);
			}
			// Clear all timed flags.
			//u8_tasks&=~(TASK_1HZ|TASK_2HZ|TASK_10HZ|TASK_50HZ|TASK_250HZ|TASK_500HZ);
		}
		if((u8_buf_interrupts&INTR_SPI_READY)!=0)
		{
			u8_buf_interrupts&=~INTR_SPI_READY;
			// Finish SPI transmittion by releasing /CS.
			SPI_TX_END;
		}
#ifdef UART_TERM
		if((u8_buf_interrupts&INTR_UART_SENT)!=0)
		{
			u8_buf_interrupts&=~INTR_UART_SENT;
			// Send data from buffer to UART if any.
			UART_send_byte();
		}
#endif /* UART_TERM */
		// Check if everything is done and MCU can sleep.
		if(u8_user_mode!=USR_MODE_STOP)
		{
			// Reset sleep inhibition timer.
			u8_sleep_inh_timer = 0;
		}
		else if((CAPSTAN_STATE==0)&&					// Capstan was stopped by timeout
			(u8_sleep_inh_timer>=SLEEP_INHIBIT_2HZ)&&	// Sleep is allowed
			(u8_transport_error==TTR_ERR_NONE))			// No pending errors
		{
			// Go to sleep.
			CPU_power_down();
		}

    }
}
