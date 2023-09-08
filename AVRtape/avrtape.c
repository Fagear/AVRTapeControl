/*
 * avrtape.c
 *
 * Created:			2021-03-16 13:17:21
 * Modified:		2023-09-07
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		Main tape transport logic implementation
 *
 */

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
uint8_t u8_mech_type=0;						// Selected type of mechanism
uint8_t u8_transition_timer=0;				// Solenoid holding timer
uint8_t u8_tacho_timer=0;					// Time from last tachometer signal
uint8_t u8_dbg_timer=0;						// Debug timer
uint8_t u8_user_mode=USR_MODE_STOP;			// User-requested mode
uint8_t u8_mech_mode=USR_MODE_STOP;			// Current user-level transport mode
uint8_t u8_last_play_dir=PB_DIR_FWD;		// Last playback direction
uint8_t u8_transport_error=TTR_ERR_NONE;	// Last transport error
uint8_t u8_features=TTR_REV_DEFAULT;		// Reverse playback settings
uint8_t u8_record=0;						// Recording settings
uint16_t u16_audio_in_left=0;				// Left channel from the ADC input
uint16_t u16_audio_in_right=0;				// Right channel from the ADC input
uint16_t u16_audio_center_left=0;			// Left channel center level
uint16_t u16_audio_center_right=0;			// Right channel center level

uint8_t u8a_spi_buf[SPI_IDX_MAX];			// Data to send via SPI bus
char u8a_buf[48];							// Buffer for UART debug messages

// Firmware description strings.
volatile const uint8_t ucaf_version[] PROGMEM = "v0.07";			// Firmware version
volatile const uint8_t ucaf_compile_time[] PROGMEM = __TIME__;		// Time of compilation
volatile const uint8_t ucaf_compile_date[] PROGMEM = __DATE__;		// Date of compilation
volatile const uint8_t ucaf_info[] PROGMEM = "ATmega tape transport controller";				// Firmware description
volatile const uint8_t ucaf_author[] PROGMEM = "Maksim Kryukov aka Fagear (fagear@mail.ru)";	// Author

//-------------------------------------- System timer interrupt handler.
ISR(SYST_INT, ISR_NAKED)
{
	INTR_IN;
	// 1000 Hz event.
	u8i_interrupts|=INTR_SYS_TICK;
	INTR_OUT;
}

ISR(TIMER0_COMPA_vect)
{
	//REC_EN_ON;
	NOP; NOP; NOP; NOP; NOP;
	//REC_EN_OFF;
}

//-------------------------------------- ADC Conversion Complete Handler.
ISR(ADC_INT/*, ISR_NAKED*/)
{
	//INTR_IN;
	u8i_interrupts|=INTR_READ_ADC;

	REC_EN_ON;
	// Store mux channel.
	u8i_last_adc_mux = ADC_INPUT_SW&ADC_MUX_MASK;
	// Keep mux register without channel bits.
	u8i_adc_new_mux = (ADC_INPUT_SW&(~ADC_MUX_MASK));
	// Select next channel.
	if(u8i_last_adc_mux==ADC_IN_LEFT_CH)
	{
		u8i_adc_new_mux += ADC_IN_RIGHT_CH;
	}
	// Apply new channel.
	ADC_INPUT_SW = u8i_adc_new_mux;
	// Store data.
	u16i_last_adc_data = ADC_DATA_16;
	REC_EN_OFF;

	//INTR_OUT;
}

//-------------------------------------- SPI data transmittion finished.
ISR(SPI_INT, ISR_NAKED)
{
	INTR_IN;
	u8i_interrupts|=INTR_SPI_READY;
	INTR_OUT;
}

//-------------------------------------- USART, Rx Complete
ISR(UART_RX_INT, ISR_NAKED)
{
	//INTR_UART_IN;
	// Receive byte.
	//UART_receive_byte();
	//INTR_UART_OUT;
	INTR_IN;
	u8i_interrupts|=INTR_UART_RECEIVED;
	INTR_OUT;
}

//-------------------------------------- USART, Tx Complete
ISR(UART_TX_INT, ISR_NAKED)
{
	INTR_IN;
	u8i_interrupts|=INTR_UART_SENT;
	INTR_OUT;
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

//-------------------------------------- Transforming audio values (centering wave and putting it into limits).
uint16_t audio_centering(uint16_t in_audio, uint16_t in_center)
{
	// Run time: us @ 8 MHz
	int16_t au_temp;
	// Current data is for filtered channel.
	au_temp = in_audio;
	// Apply wave offset (full wave rectification).
	if(au_temp<in_center)
	{
		// Invert negative wave value and shift center of wave to zero.
		au_temp = in_center - au_temp;
	}
	else
	{
		// Shift center of wave to zero.
		au_temp = au_temp - in_center;
	}
	// Put wave in limits.
	if(au_temp<0) au_temp = 0;
	return (uint16_t)au_temp;
}

//-------------------------------------- Read data from ADC.
void ADC_read_result(void)
{
	uint8_t temp_ch;
	uint16_t temp_data;

	// Buffer data.
	ADC_DIS_INTR;
	temp_ch = u8i_last_adc_mux;
	temp_data = u16i_last_adc_data;
	ADC_EN_INTR;

	if(temp_ch==ADC_IN_LEFT_CH)
	{
		u16_audio_in_left = temp_data;
	}
	else
	{
		u16_audio_in_right = temp_data;
	}
}

//-------------------------------------- Find center of audio wave.
void audio_input_calibrate(void)
{
	uint32_t tst_lch_avg=0;
	uint32_t tst_rch_avg=0;
	int16_t var_temp=8960;
	// Collect 4096 audio samples.
	while(var_temp>0)
	{
		// Disable interrupts globally.
		cli();
		// Buffer all interrupts.
		u8_buf_interrupts|=u8i_interrupts;
		// Clear all interrupt flags.
		u8i_interrupts=0;
		// Enable interrupts globally.
		sei();
		// Wait for ADC conversion.
		if((u8_buf_interrupts&INTR_READ_ADC)!=0)
		{
			// Read data from analog inputs (channels are interleaved).
			ADC_read_result();
			// Start collection data after ~1s (discard startup transitions).
			if(var_temp<4097)
			{
				if(u16i_last_adc_data==ADC_IN_LEFT_CH)
				{
					// Store sum of all samples.
					tst_lch_avg += u16_audio_in_left;
				}
				else
				{
					// Store sum of all samples.
					tst_rch_avg += u16_audio_in_right;
				}
			}
			var_temp--;
			// Reset watchdog timer.
			wdt_reset();
			// ADC conversion done.
			u8_buf_interrupts&=~INTR_READ_ADC;
		}
	}
	// Calculate center of waves (average of 4096/2 samples).
	u16_audio_center_left = ((tst_lch_avg>>11)&(0x3FF));	// summ/2048
	u16_audio_in_right = ((tst_rch_avg>>11)&(0x3FF));		// summ/2048

	// Vin = ADC*5/1024.
	// Correct value if too much offset (<0.12V or >4.88).
	/*if((ui_audio_lf_center<25)||(ui_audio_lf_center>1000))
	{
		// Set default center of wave (~1.95V input).
		ui_audio_lf_center=399;
	}*/

	// Re-center last audio to make it valid.
	u16_audio_in_left = audio_centering(u16_audio_in_left, u16_audio_center_left);
	u16_audio_in_right = audio_centering(u16_audio_in_right, u16_audio_center_right);
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
	if(SW_NOREC_FWD_STATE==0)
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
	if(SW_NOREC_REV_STATE==0)
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
	/*sprintf(u8a_buf, "SWS|>0x%02x<|0x%02x|0x%02x\n\r",
				sw_state, sw_pressed, sw_released);
	UART_add_string(u8a_buf);*/
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
	/*sprintf(u8a_buf, "KBD|>0x%02x<|0x%02x|0x%02x\n\r",
				kbd_state, kbd_pressed, kbd_released);
	UART_add_string(u8a_buf);*/
	if(kbd_pressed!=0)
	{
		sprintf(u8a_buf, "KBD|REWN:%01d|STOP:%01d|FFWD:%01d|PLAY:%01d|REC:%01d\n\r",
				((kbd_pressed&USR_BTN_REWIND)==0)?0:1,
				((kbd_pressed&USR_BTN_STOP)==0)?0:1,
				((kbd_pressed&USR_BTN_FFORWARD)==0)?0:1,
				((kbd_pressed&USR_BTN_PLAY)==0)?0:1,
				((kbd_pressed&USR_BTN_RECORD)==0)?0:1);
		UART_add_string(u8a_buf);
	}
}

//-------------------------------------- Poll tachometer transitions.
inline void poll_tacho(void)
{
	if(((sw_pressed&TTR_SW_TACHO)!=0)||((sw_released&TTR_SW_TACHO)!=0))
	{
		// Tachometer has changed its state from last time.
		sw_pressed&=~TTR_SW_TACHO;
		sw_released&=~TTR_SW_TACHO;
		u8_tacho_timer = 0;
	}
	else if(u8_tacho_timer<240)
	{
		// Count delay up if possible.
		u8_tacho_timer++;
	}
}

//-------------------------------------- Update indication.
inline void update_indicators(void)
{
	/*if((sw_state&TTR_SW_TAPE_IN)!=0)
	{
		LED_PORT|=LED_GREEN;
	}
	else
	{
		LED_PORT&=~LED_GREEN;
	}*/

	if((sw_state&TTR_SW_STOP)!=0)
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_STOP;
	}
	else
	{
		u8a_spi_buf[SPI_IDX_IND] &= ~IND_STOP;
	}

	if((sw_state&TTR_SW_TACHO)!=0)
	{
		u8a_spi_buf[SPI_IDX_IND] |= IND_TACHO;
	}
	else
	{
		u8a_spi_buf[SPI_IDX_IND] &= ~IND_TACHO;
	}
	if(u8_transport_error==TTR_ERR_NONE)
	{
		// No transport error, normal operation.
		u8a_spi_buf[SPI_IDX_IND] &= ~IND_ERROR;

		if(u8_mech_mode==USR_MODE_STOP)
		{
			if(u8_transition_timer==0)
			{
				u8a_spi_buf[SPI_IDX_IND] |= IND_STOP;
			}
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
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_STOP;
		}

		// Playback forward indicator.
		if(u8_mech_mode==USR_MODE_PLAY_FWD)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_FWD;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY_FWD;
		}

		// Playback backwards indicator.
		if(u8_mech_mode==USR_MODE_PLAY_REV)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_REV;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY_REV;
		}

		// Fast forward indicator.
		// TODO: turn on playback as well
		if(u8_mech_mode==USR_MODE_FWIND_FWD)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_FFORWARD;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_FFORWARD;
		}

		// Rewind indicator.
		// TODO: turn on playback as well
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
		u8a_spi_buf[SPI_IDX_IND] &= ~(IND_TACHO|IND_PLAY_FWD|IND_PLAY_REV|IND_FFORWARD|IND_REWIND);
		if((u8_transport_error&TTR_ERR_BAD_DRIVE)!=0)
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
		else if((u8_transport_error&TTR_ERR_NO_CTRL)!=0)
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
		else
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_ERROR;
		}
		
	}

	// Transmit indicator information via SPI.
	SPI_TX_START;
	SPI_send_byte(u8a_spi_buf[SPI_IDX_IND]);
}

//-------------------------------------- Process input from user.
void process_user(void)
{
	uint8_t last_user_mode;
	// Save mode.
	last_user_mode = u8_user_mode;
	if((kbd_pressed&USR_BTN_FFORWARD)!=0)
	{
		kbd_pressed&=~USR_BTN_FFORWARD;
		// Fast forward.
		u8_user_mode = USR_MODE_FWIND_FWD;
	}
	if((kbd_pressed&USR_BTN_PLAY)!=0)
	{
		kbd_pressed&=~USR_BTN_PLAY;
		// Check reverse settings.
		if((u8_features&TTR_FEA_REV_ENABLE)==0)
		{
			// Reverse operations are disabled.
			// Start/resume playback in forward direction.
			u8_user_mode = USR_MODE_PLAY_FWD;
		}
		else
		{
			// Check if current mode is playback already and check current direction.
			// TODO: maybe remove this and only check [u8_last_play_dir].
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
		}
	}
	if((kbd_pressed&USR_BTN_STOP)!=0)
	{
		kbd_pressed&=~USR_BTN_STOP;
		// Stop the tape.
		u8_user_mode = USR_MODE_STOP;
	}
	if((kbd_pressed&USR_BTN_RECORD)!=0)
	{
		kbd_pressed&=~USR_BTN_RECORD;
		// TODO: add record button processing
	}
	if((kbd_pressed&USR_BTN_REWIND)!=0)
	{
		kbd_pressed&=~USR_BTN_REWIND;
		// Rewind.
		u8_user_mode = USR_MODE_FWIND_REV;
	}
	if(last_user_mode!=u8_user_mode)
	{
		// Log user mode change.
		UART_add_flash_string((uint8_t *)cch_new_user_mode);
		UART_dump_user_mode(last_user_mode); UART_add_flash_string((uint8_t *)cch_arrow); UART_dump_user_mode(u8_user_mode);
		UART_add_flash_string((uint8_t *)cch_endl);
	}
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
			u8_transition_timer = TIM_TANA_DLY_PB_WAIT;
		}
		else if(u8_user_mode==USR_MODE_FWIND_REV)
		{
			// Preset time for full cycle from PLAY to FAST WIND.
			u8_transition_timer = TIM_TANA_DLY_FWIND_WAIT;
		}
		else if(u8_user_mode==USR_MODE_FWIND_FWD)
		{
			// Preset time for full cycle from PLAY to FAST WIND.
			u8_transition_timer = TIM_TANA_DLY_FWIND_WAIT;
		}
		u8_user_mode = USR_MODE_STOP;
	}

	if(u8_transition_timer>0)
	{
		uint8_t sol_state;
		sol_state = 0;
		if(SOLENOID_STATE!=0) sol_state = 1;
		sprintf(u8a_buf, "MODE|>%03u<|%02x|%01x\n\r", u8_transition_timer, sw_state, sol_state);
		UART_add_string(u8a_buf);

		if(u8_user_mode==USR_MODE_PLAY_FWD)
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
		}
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
	UART_dump_out();
}

//-------------------------------------- Main function.
int main(void)
{
	// Start-up initialization.
	system_startup();

	// Default mech.
	u8_mech_type = TTR_TYPE_CRP42602Y;

	// Init modes to selected transport.
	if(u8_mech_type==TTR_TYPE_CRP42602Y)
	{
		u8_user_mode = USR_MODE_STOP;
	}
	else if(u8_mech_type==TTR_TYPE_TANASHIN)
	{
		u8_user_mode = USR_MODE_STOP;
		u8_features &= ~(TTR_FEA_REV_ENABLE|TTR_FEA_PB_AUTOREV|TTR_FEA_PB_LOOP);	// Disable reverse functions for non-reverse mech.
	}

	// Output startup messages.
	UART_add_flash_string((uint8_t *)cch_startup_1);
	UART_add_flash_string((uint8_t *)ucaf_info); UART_add_flash_string((uint8_t *)cch_endl);
	UART_add_flash_string((uint8_t *)ucaf_version); UART_add_string(" ["); UART_add_flash_string((uint8_t *)ucaf_compile_date); UART_add_string(", "); UART_add_flash_string((uint8_t *)ucaf_compile_time); UART_add_string("]"); UART_add_flash_string((uint8_t *)cch_endl);
	UART_add_flash_string((uint8_t *)ucaf_author); UART_add_flash_string((uint8_t *)cch_endl); UART_dump_out();
	UART_add_flash_string((uint8_t *)cch_endl); UART_dump_settings(u8_features); UART_add_flash_string((uint8_t *)cch_endl);
	UART_dump_out();

	// Start SPI comms.
	SPI_int_enable();
	SPI_TX_START;
	SPI_send_byte(0x00);

	// Enable interrupts globally.
	sei();
	//ADC_CLR_INTR;
	//ADC_START;

	// Calibrate audio inputs.
	//audio_input_calibrate();

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
		/*if((u8_buf_interrupts&INTR_READ_ADC)!=0)
		{
			u8_buf_interrupts&=~INTR_READ_ADC;
			// ADC conversion is done.
			ADC_read_result();
			// Transforming audio values (centering wave and putting it into limits).
			u16_audio_in_left = audio_centering(u16_audio_in_left, u16_audio_center_left);
			u16_audio_in_right = audio_centering(u16_audio_in_right, u16_audio_center_right);
		}*/
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
				u8_buf_interrupts|=INTR_UART_SENT;
			}
			if((u8_tasks&TASK_10HZ)!=0)
			{
				u8_tasks&=~TASK_10HZ;
				// 10 Hz event, 100 ms period.
				// Toggle fast blink flag.
				u8_tasks^=TASK_FAST_BLINK;
			}
			if((u8_tasks&TASK_50HZ)!=0)
			{
				u8_tasks&=~TASK_50HZ;
				// 50 Hz event, 20 ms period.
				// Scan user keys.
				keys_simple_scan();
				// Check tachometer.
				poll_tacho();
				// Process user input.
				process_user();
			}
			if((u8_tasks&TASK_500HZ)!=0)
			{
				u8_tasks&=~TASK_500HZ;
				// 500 Hz event, 2 ms period.
				// Scan switches and sensors.
				switches_scan();
				// Update transport state machine and solenoid action.
				if(u8_mech_type==TTR_TYPE_CRP42602Y)
				{
					mech_crp42602y_state_machine(u8_features, sw_state, &u8_tacho_timer, &u8_user_mode, &u8_last_play_dir);
					u8_mech_mode = mech_crp42602y_get_mode();
					u8_transition_timer = mech_crp42602y_get_transition();
					u8_transport_error = mech_crp42602y_get_error();
				}
				else if(u8_mech_type==TTR_TYPE_TANASHIN)
				{
					mech_tanashin_state_machine(u8_features, sw_state, &u8_tacho_timer, &u8_user_mode, &u8_last_play_dir);
					u8_mech_mode = mech_tanashin_get_mode();
					u8_transition_timer = mech_tanashin_get_transition();
					u8_transport_error = mech_tanashin_get_error();
				}
				else
				{
					mech_log();
				}
				// Update LEDs.
				update_indicators();
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
		if((u8_buf_interrupts&INTR_UART_RECEIVED)!=0)
		{
			u8_buf_interrupts&=~INTR_UART_RECEIVED;
			// Receive a byte from UART.
			UART_receive_byte();
		}
		if((u8_buf_interrupts&INTR_UART_SENT)!=0)
		{
			u8_buf_interrupts&=~INTR_UART_SENT;
			// Send data from buffer to UART if any.
			UART_send_byte();
		}
    }
}

