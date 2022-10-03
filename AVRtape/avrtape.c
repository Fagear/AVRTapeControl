/*
 * avrtape.c
 *
 * Created:			2021-03-16 13:17:21
 * Modified:		2021-04-16
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		Main tape transport logic implementation
 *
 */ 

#include "avrtape.h"

volatile uint8_t u8i_interrupts=0;		// Deferred interrupts call flags (non-buffered)
uint16_t u16i_last_adc_data=0;			// ADC data at last interrupt
uint8_t u8i_last_adc_mux=0;				// ADC mux settings at last interrupt
uint8_t u8i_adc_new_mux=0;				// New mux for ADC next conversion
uint8_t u8_buf_interrupts=0;			// Deferred interrupts call flags (buffered)
uint8_t u8_tasks=0;						// Deferred tasks call flags
uint8_t u8_500hz_cnt=0;					// Divider for 500 Hz
uint8_t u8_50hz_cnt=0;					// Divider for 50 Hz
uint8_t u8_10hz_cnt=0;					// Divider for 10 Hz
uint8_t u8_2hz_cnt=0;					// Divider for 2 Hz
uint8_t u8_mech_type=0;					// Selected type of mechanism
uint8_t u8_cycle_timer=0;				// Solenoid holding timer
uint8_t u8_tacho_timer=0;				// Time from last tachometer signal
uint8_t u8_dbg_timer=0;					// Debug timer
uint8_t u8_user_mode=0;					// User-requested mode
uint8_t u8_target_trr_mode=0;			// Target transport mode (derived from [u8_user_mode])
uint8_t u8_transport_mode=0;			// Current tape transport mode (transitions to [u8_target_trr_mode])
uint8_t u8_last_play_dir=PB_DIR_FWD;		// Last playback direction
uint8_t u8_transport_error=TTR_ERR_NONE;	// Last transport error
uint8_t u8_reverse=TTR_REV_DEFAULT;			// Reverse playback settings
uint8_t u8_record=0;						// Recording settings
uint16_t u16_audio_in_left=0;				// Left channel from the ADC input
uint16_t u16_audio_in_right=0;				// Right channel from the ADC input
uint16_t u16_audio_center_left=0;			// Left channel center level
uint16_t u16_audio_center_right=0;			// Right channel center level

uint8_t u8a_spi_buf[SPI_IDX_MAX];			// Data to send via SPI bus
char u8a_buf[48];							// Buffer for UART debug messages

// Firmware description strings.
volatile const uint8_t ucaf_version[] PROGMEM = "v0.06";			// Firmware version
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
	
	if(u8_transport_error==TTR_ERR_NONE)
	{
		// No transport error, normal operation.
		u8a_spi_buf[SPI_IDX_IND] &= ~IND_ERROR;
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
		
		if(u8_target_trr_mode==TTR_42602_MODE_STOP)
		{
			if(u8_cycle_timer==0)
			{
				u8a_spi_buf[SPI_IDX_IND] |= (1<<1);
			}
			else if((u8_tasks&TASK_FAST_BLINK)!=0)
			{
				u8a_spi_buf[SPI_IDX_IND] |= (1<<1);
			}
			else
			{
				u8a_spi_buf[SPI_IDX_IND] &= ~(1<<1);
			}
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~(1<<1);
		}
		
		// Playback forward indicator.
		if(u8_target_trr_mode==TTR_42602_MODE_PB_FWD)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_FWD;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY_FWD;
		}
		
		// Playback backwards indicator.
		if(u8_target_trr_mode==TTR_42602_MODE_PB_REV)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_PLAY_REV;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_PLAY_REV;
		}
		
		// Fast forward indicator.
		// TODO: turn on playback as well
		if(u8_target_trr_mode==TTR_42602_MODE_FW_FWD)
		{
			u8a_spi_buf[SPI_IDX_IND] |= IND_FFORWARD;
		}
		else
		{
			u8a_spi_buf[SPI_IDX_IND] &= ~IND_FFORWARD;
		}
		
		// Rewind indicator.
		// TODO: turn on playback as well
		if(u8_target_trr_mode==TTR_42602_MODE_FW_REV)
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
	}
	
	// Transmit indicator information via SPI.
	SPI_TX_START;
	SPI_send_byte(u8a_spi_buf[SPI_IDX_IND]);	
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

//-------------------------------------- Print user mode alias.
void UART_dump_user_mode(uint8_t in_mode)
{
	if(in_mode==USR_MODE_STOP)
	{
		UART_add_flash_string((uint8_t *)cch_mode_stop);
	}
	else if(in_mode==USR_MODE_PLAY_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_pb_fwd);
	}
	else if(in_mode==USR_MODE_PLAY_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_pb_rev);
	}
	else if(in_mode==USR_MODE_FWIND_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_fwd);
	}
	else if(in_mode==USR_MODE_FWIND_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_rev);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_mode_unknown);
	}
}

//-------------------------------------- Print CRP42602Y transport mode alias.
void UART_dump_CRP42602Y_mode(uint8_t in_mode)
{
	if(in_mode==TTR_42602_MODE_STOP)
	{
		UART_add_flash_string((uint8_t *)cch_mode_stop);
	}
	else if(in_mode==TTR_42602_MODE_PB_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_pb_fwd);
	}
	else if(in_mode==TTR_42602_MODE_PB_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_pb_rev);
	}
	else if(in_mode==TTR_42602_MODE_FW_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_fwd);
	}
	else if(in_mode==TTR_42602_MODE_FW_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_rev);
	}
	else if(in_mode==TTR_42602_MODE_HALT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_halt);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_mode_unknown);
	}
}

//-------------------------------------- Print Tanashin transport mode alias.
void UART_dump_Tanashin_mode(uint8_t in_mode)
{
	if(in_mode==TTR_TANA_MODE_TO_INIT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_to_init);
	}
	else if(in_mode==TTR_TANA_MODE_INIT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_init);
	}
	else if(in_mode==TTR_TANA_MODE_TO_STOP)
	{
		UART_add_flash_string((uint8_t *)cch_mode_to_stop);
	}
	else if(in_mode==TTR_TANA_MODE_STOP)
	{
		UART_add_flash_string((uint8_t *)cch_mode_stop);
	}
	else if(in_mode==TTR_TANA_MODE_PB_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_pb_fwd);
	}
	else if(in_mode==TTR_TANA_MODE_FW_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_fwd);
	}
	else if(in_mode==TTR_TANA_MODE_FW_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_rev);
	}
	else if(in_mode==TTR_TANA_MODE_TO_HALT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_to_halt);
	}
	else if(in_mode==TTR_TANA_MODE_HALT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_halt);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_mode_unknown);
	}
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
		//u8_user_mode = TTR_42602_MODE_FW_FWD;
		u8_user_mode = USR_MODE_FWIND_FWD;
	}
	if((kbd_pressed&USR_BTN_PLAY)!=0)
	{
		kbd_pressed&=~USR_BTN_PLAY;
		// Check reverse settings.
		if((u8_reverse&TTR_REV_ENABLE)==0)
		{
			// Reverse operations are disabled.
			// Start/resume playback in forward direction.
			//u8_user_mode = TTR_42602_MODE_PB_FWD;
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
		//u8_user_mode = TTR_42602_MODE_STOP;
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
		//u8_user_mode = TTR_42602_MODE_FW_REV;
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

//-------------------------------------- Perform tape transport state machine for CRP42602Y tape mech.
void mech_crp42602y_state_machine(void)
{
	if((sw_state&TTR_SW_TAPE_IN)==0)
	{
		// Reset last played direction if tape is removed.
		u8_last_play_dir = PB_DIR_FWD;
		if((u8_target_trr_mode!=TTR_42602_MODE_STOP)||(u8_user_mode!=USR_MODE_STOP))
		{
			UART_add_flash_string((uint8_t *)cch_no_tape);
		}
		// Tape is out, clear any active mode.
		u8_target_trr_mode = TTR_42602_MODE_STOP;
		// Clear user mode.
		u8_user_mode = USR_MODE_STOP;
	}
	if(u8_cycle_timer==0)
	{
		// Transport is in stable state (mode transition finished).
		if(u8_transport_mode==TTR_42602_MODE_HALT)
		{
			// Transport control is halted due to an error.
			u8_target_trr_mode = TTR_42602_MODE_HALT;
			u8_user_mode = USR_MODE_STOP;
			// Keep timer reset.
			u8_cycle_timer = 0;
			// Keep solenoid inactive.
			SOLENOID_OFF;
			// Check mechanical stop sensor.
			if((sw_state&TTR_SW_STOP)==0)
			{
				// Transport is not in STOP mode.
				UART_add_flash_string((uint8_t *)cch_halt_force_stop);
				// Force STOP if transport is not in STOP.
				u8_cycle_timer = TTR_42602_DELAY_STOP;
				// Pull solenoid in to initiate mode change.
				SOLENOID_ON;
			}
		}
		else if(u8_target_trr_mode!=u8_transport_mode)
		{
			UART_add_string("TARGET!=CURRENT : "); UART_dump_CRP42602Y_mode(u8_target_trr_mode); UART_add_string(" != "); UART_dump_CRP42602Y_mode(u8_transport_mode); UART_add_flash_string((uint8_t *)cch_endl);
			// Target mode is not the same as current transport mode (need to start transition to another mode).
			if(u8_target_trr_mode==TTR_42602_MODE_TO_INIT)
			{
				// Target mode: start-up delay.
				UART_add_flash_string((uint8_t *)cch_startup_delay);
				u8_cycle_timer = TTR_42602_DELAY_STOP;
				u8_transport_mode = TTR_42602_MODE_INIT;
				u8_target_trr_mode = TTR_42602_MODE_STOP;
				u8_user_mode = USR_MODE_STOP;
				SOLENOID_OFF;
			}
			else if(u8_target_trr_mode==TTR_42602_MODE_STOP)
			{
				// Target mode: full stop.
				UART_add_string("New: stop\n\r");
				u8_cycle_timer = TTR_42602_DELAY_STOP;
				u8_transport_mode = TTR_42602_MODE_TO_STOP;
				// Pull solenoid in to initiate mode change.
				SOLENOID_ON;
			}
			else
			{
				// Reset last error.
				u8_transport_error = TTR_ERR_NONE;
				// Check new target mode.
				if((u8_target_trr_mode==TTR_42602_MODE_PB_FWD)||(u8_target_trr_mode==TTR_42602_MODE_PB_REV)
					||(u8_target_trr_mode==TTR_42602_MODE_FW_FWD)||(u8_target_trr_mode==TTR_42602_MODE_FW_REV)
					||(u8_target_trr_mode==TTR_42602_MODE_FW_FWD_HD_REV)||(u8_target_trr_mode==TTR_42602_MODE_FW_REV_HD_REV))
				{
					// Start transition to active mode.
					UART_add_string("New: active\n\r");
					u8_cycle_timer = TTR_42602_DELAY_RUN;
					u8_transport_mode = TTR_42602_MODE_TO_START;
					// Pull solenoid in to initiate mode change.
					SOLENOID_ON;
				}
				else
				{
					// Unknown mode, reset to STOP.
					UART_add_flash_string((uint8_t *)cch_unknown_mode);
					u8_target_trr_mode = TTR_42602_MODE_STOP;
					u8_user_mode = USR_MODE_STOP;
				}
			}
		}
		else if(u8_user_mode!=u8_target_trr_mode)
		{
			UART_add_string("USER!=TARGET : "); UART_dump_CRP42602Y_mode(u8_user_mode); UART_add_string(" != "); UART_dump_CRP42602Y_mode(u8_target_trr_mode); UART_add_flash_string((uint8_t *)cch_endl);
			// Not in disabled state, target mode is reached.
			// User wants another mode than current transport target is.
			if(u8_target_trr_mode!=TTR_42602_MODE_STOP)
			{
				// Mechanism is in active mode, user wants another mode, set target as STOP, it's the only way to transition to another mode. 
				// New target mode will apply in the next run of the [mech_crp42602y_state_machine()].
				u8_target_trr_mode = TTR_42602_MODE_STOP;
				UART_add_string("New: stop (u)\n\r");
			}
			else
			{
				// Mechanism is in STOP mode, simple: set target to what user wants.
				// New target mode will apply in the next run of the [mech_crp42602y_state_machine()].
				u8_target_trr_mode = u8_user_mode;
				UART_add_string("New: active (u)\n\r");
			}
		}
		else
		{
			// Transport is not due to transition through modes (u8_transport_mode == u8_target_trr_mode == u8_user_mode).
			if(u8_transport_mode==TTR_42602_MODE_STOP)
			{
				// Transport supposed to be in STOP.
				// Check mechanism for mechanical STOP condition.
				if((sw_state&TTR_SW_STOP)==0)
				{
					// Transport is not in STOP mode.
					UART_add_flash_string((uint8_t *)cch_stop_active);
					// Force STOP if transport is not in STOP.
					u8_cycle_timer = TTR_42602_DELAY_STOP;
					u8_transport_mode = TTR_42602_MODE_TO_STOP;
					// Pull solenoid in to initiate mode change.
					SOLENOID_ON;
				}
				else if(u8_tacho_timer>TACHO_42602_STOP_DLY_MAX)
				{
					// No signal from takeup tachometer for too long.
					UART_add_flash_string((uint8_t *)cch_bad_drive1);
					UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_bad_drive2);
					// No motor drive or bad belts, register an error.
					u8_transport_mode = TTR_42602_MODE_HALT;
					u8_transport_error += TTR_ERR_BAD_DRIVE;
				}
			}
			else if((u8_transport_mode==TTR_42602_MODE_PB_FWD)||(u8_transport_mode==TTR_42602_MODE_PB_REV))
			{
				// Transport supposed to be in PLAYBACK.
				if(u8_tacho_timer>TACHO_42602_PLAY_DLY_MAX)
				{
					// No signal from takeup tachometer for too long.
					// Perform auto-stop.
					u8_target_trr_mode = TTR_42602_MODE_STOP;
					// Set default "last playback" as forward, it will be corrected below if required.
					u8_last_play_dir = PB_DIR_FWD;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
					// Check if reverse functions are enabled.
					if((u8_reverse&TTR_REV_ENABLE)!=0)
					{
						// Reverse functions are allowed.
						if((u8_reverse&TTR_REV_PB_AUTO)!=0)
						{
							// Auto-reverse is allowed.
							if(u8_transport_mode==TTR_42602_MODE_PB_FWD)
							{
								//UART_add_string("No PB tacho, auto-reverse FWD->REV queued\n\r");
								UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_fwd_rev);
								// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
								u8_user_mode = USR_MODE_PLAY_REV;
							}
							else if((u8_transport_mode==TTR_42602_MODE_PB_REV)&&((u8_reverse&TTR_REV_PB_LOOP)!=0))
							{
								//UART_add_string("No PB tacho, auto-reverse REV->FWD queued\n\r");
								UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_rev_fwd);
								// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
								u8_user_mode = USR_MODE_PLAY_FWD;
							}
							else
							{
								//UART_add_string("No PB tacho, auto-stop at the end\n\r");
								UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
								// Stop mode already queued.
							}
						}
						else
						{
							// Auto-reverse is disabled.
							if(u8_transport_mode==TTR_42602_MODE_PB_REV)
							{
								// Playback was in reverse direction.
								//UART_add_string("No PB tacho, auto-stop at the end\n\r");
								UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
								// Make next playback direction in reverse direction after auto-stop.
								u8_last_play_dir = PB_DIR_REV;
								// Stop mode already queued.
							}
							else if((u8_reverse&TTR_REV_REW_AUTO)!=0)
							{
								// Playback was in forward direction.
								// Auto-rewind is enabled.
								//UART_add_string("No PB tacho, auto-rewind queued\n\r");
								UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_rewind);
								// Queue rewind.
								u8_user_mode = USR_MODE_FWIND_REV;
							}
							else
							{
								// No auto-reverse or auto-rewind.
								//UART_add_string("No PB tacho, auto-stop\n\r");
								UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
								// Stop mode already queued.
							}
						}
					}
					else if((u8_reverse&TTR_REV_REW_AUTO)!=0)
					{
						// No reverse operations and auto-rewind is enabled.
						//UART_add_string("No PB tacho, auto-rewind queued\n\r");
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_rewind);
						// Queue rewind.
						u8_user_mode = USR_MODE_FWIND_REV;
					}
					else
					{
						// No reverse operations and no auto-rewind.
						//UART_add_string("No PB tacho, auto-stop\n\r");
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
						// Stop mode already queued.
					}
				}
				if((sw_state&TTR_SW_STOP)!=0)
				{
					// Mechanism unexpectedly slipped into STOP.
					UART_add_flash_string((uint8_t *)cch_stop_corr);
					// Correct logic mode.
					u8_transport_mode = TTR_42602_MODE_STOP;
					u8_target_trr_mode = TTR_42602_MODE_STOP;
					u8_user_mode = USR_MODE_STOP;
					u8_cycle_timer = 0;
				}
			}
			else if((u8_transport_mode==TTR_42602_MODE_FW_FWD)||(u8_transport_mode==TTR_42602_MODE_FW_REV)||(u8_transport_mode==TTR_42602_MODE_FW_FWD_HD_REV)||(u8_transport_mode==TTR_42602_MODE_FW_REV_HD_REV))
			{
				// Transport supposed to be in FAST WIND.
				if(u8_tacho_timer>TACHO_42602_FWIND_DLY_MAX)
				{
					// No signal from takeup tachometer for too long.
					// Perform auto-stop.
					u8_target_trr_mode = TTR_42602_MODE_STOP;
					// Set default "last playback" as forward, it will be corrected below if required.
					u8_last_play_dir = PB_DIR_FWD;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
					// Check if reverse functions are enabled.
					if((u8_reverse&TTR_REV_ENABLE)!=0)
					{
						if((u8_transport_mode==TTR_42602_MODE_FW_REV)||(u8_transport_mode==TTR_42602_MODE_FW_REV_HD_REV))
						{
							// Fast wind was in reverse direction.
							//UART_add_string("No FW tacho, auto-stop at the end\n\r");
							UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
							// Make next playback direction in reverse direction after auto-stop.
							u8_last_play_dir = PB_DIR_REV;
						}
						else if((u8_reverse&TTR_REV_REW_AUTO)!=0)
						{
							// Fast wind was in forward direction.
							//UART_add_string("No FW tacho, auto-rewind queued\n\r");
							UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_rewind);
							// Auto-rewind is enabled.
							// Set mode to rewind.
							u8_user_mode = USR_MODE_FWIND_REV;
						}
					}
					else
					{
						//UART_add_string("No FW tacho, auto-stop\n\r");
						UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
					}
				}
				if((sw_state&TTR_SW_STOP)!=0)
				{
					// Mechanism unexpectedly slipped into STOP.
					UART_add_flash_string((uint8_t *)cch_stop_corr);
					// Correct logic mode.
					u8_transport_mode = TTR_42602_MODE_STOP;
					u8_target_trr_mode = TTR_42602_MODE_STOP;
					u8_user_mode = USR_MODE_STOP;
					u8_cycle_timer = 0;
				}
			}
		}
	}
	else
	{
		// Count down mode transition timer.
		u8_cycle_timer--;
		if(u8_cycle_timer==0)
		{
			// Desired mode reached.
			// Release solenoid.
			SOLENOID_OFF;
			if(u8_transport_mode!=TTR_42602_MODE_HALT)
			{
				//sprintf(u8a_buf, "TRFIN|%01u>%01u\n\r", u8_transport_mode, u8_target_trr_mode);
				UART_add_string("Mode transition done: "); UART_dump_CRP42602Y_mode(u8_transport_mode); UART_add_string(" > "); UART_dump_CRP42602Y_mode(u8_target_trr_mode); UART_add_flash_string((uint8_t *)cch_endl);
				
				// Save new logic state.
				u8_transport_mode = u8_target_trr_mode;
				// Check if mechanism successfully reached target logic state.
				// Check if target was one of the active modes.
				if((u8_target_trr_mode==TTR_42602_MODE_PB_FWD)||(u8_target_trr_mode==TTR_42602_MODE_PB_REV)
					||(u8_target_trr_mode==TTR_42602_MODE_FW_FWD)||(u8_target_trr_mode==TTR_42602_MODE_FW_REV)
					||(u8_target_trr_mode==TTR_42602_MODE_FW_FWD_HD_REV)||(u8_target_trr_mode==TTR_42602_MODE_FW_REV_HD_REV))
				{
					// Check if mechanical STOP state wasn't cleared.
					if((sw_state&TTR_SW_STOP)!=0)
					{
						// Mechanically mode didn't change from STOP, register an error.
						u8_transport_mode = TTR_42602_MODE_HALT;
						u8_transport_error += TTR_ERR_NO_CTRL;
						//UART_add_string("Reached active but STOP is present, halting...\n\r");
						UART_add_flash_string((uint8_t *)cch_halt_stop1);
						UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
					}
					else
					{
						// Update last playback direction.
						if(u8_target_trr_mode==TTR_42602_MODE_PB_FWD)
						{
							u8_last_play_dir = PB_DIR_FWD;
						}
						else if(u8_target_trr_mode==TTR_42602_MODE_PB_REV)
						{
							u8_last_play_dir = PB_DIR_REV;
						}
					}
				}
			}
			// Reset tachometer timer.
			u8_tacho_timer = 0;
		}
		else
		{
			// Still in transition (counter != 0).
			if(u8_transport_mode==TTR_42602_MODE_HALT)
			{
				// Desired mode: recovery stop in halt mode.
				if(u8_cycle_timer<=(TTR_42602_DELAY_STOP-TIM_42602_DLY_STOP))
				{
					// Release solenoid.
					SOLENOID_OFF;
				}
			}
			else if(u8_transport_mode==TTR_42602_MODE_INIT)
			{
				// Start-up delay.
				// Keep solenoid off.
				SOLENOID_OFF;
			}
			else if(u8_target_trr_mode==TTR_42602_MODE_STOP)
			{
				// Desired mode: stop.
				if(u8_cycle_timer<=(TTR_42602_DELAY_STOP-TIM_42602_DLY_STOP))
				{
					// Mechanism started transition to STOP mode, wait for it.
					u8_transport_mode = TTR_42602_MODE_WAIT_STOP;
					// Release solenoid.
					SOLENOID_OFF;
				}
			}
			/*
			else if(MODE)
			{
				// Mode template.
				if(u8_cycle_timer==(TTR_CRP42602_DELAY_RUN-TIM_CRP42602_DLY_WAIT_HEAD))
				{
					// Mode change started, wait for pinch direction region.
					u8_transport_mode = TTR_MODE_WAIT_DIR;
					// Release solenoid.
					SOLENOID_OFF;
				}
				else if(u8_cycle_timer==(TTR_CRP42602_DELAY_RUN-TIM_CRP42602_DLY_HEAD_DIR))
				{
					// Pinch/head direction range.
					u8_transport_mode = TTR_MODE_HD_DIR_SEL;
					// Keep solenoid off for head in forward direction.
					// Keep solenoid in for head in reverse direction.
				}
				else if(u8_cycle_timer==(TTR_CRP42602_DELAY_RUN-TIM_CRP42602_DLY_WAIT_PINCH))
				{
					// Pinch direction selection finished, wait for pinch range.
					u8_transport_mode = TTR_MODE_WAIT_PINCH;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_CRP42602_DELAY_RUN-TIM_CRP42602_DLY_PINCH_EN))
				{
					// Pinch engage range.
					u8_transport_mode = TTR_MODE_PINCH_SEL;
					// Pull solenoid in to enable pinch roller.
					// Keep solenoid off to select fast winding.
				}
				else if(u8_cycle_timer==(TTR_CRP42602_DELAY_RUN-TIM_CRP42602_DLY_WAIT_TAKEUP))
				{
					// Pinch roller engaged, wait for pickup direction range.
					u8_transport_mode = TTR_MODE_WAIT_TAKEUP;
					// Release solenoid.
				}
				else if(u8_cycle_timer==(TTR_CRP42602_DELAY_RUN-TIM_CRP42602_DLY_TAKEUP_DIR))
				{
					// Takeup direction range.
					u8_transport_mode = TTR_MODE_TU_DIR_SEL;
					// Pull solenoid in to select takeup in forward direction.
					// Keep solenoid off for takeup in reverse direction
				}
				else if(u8_cycle_timer==(TTR_CRP42602_DELAY_RUN-TIM_CRP42602_DLY_WAIT_MODE))
				{
					// Cyclogram finished, waiting for transport to reach stable state.
					u8_transport_mode = TTR_MODE_WAIT_RUN;
					// Release solenoid.
					SOLENOID_OFF;
				}
			}
			*/
			else if(u8_target_trr_mode==TTR_42602_MODE_PB_FWD)
			{
				// Desired mode: Playback in forward direction.
				if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_HEAD))
				{
					// Mode change started, wait for pinch direction region.
					u8_transport_mode = TTR_42602_MODE_WAIT_DIR;
					// Release solenoid.
					SOLENOID_OFF;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_HEAD_DIR))
				{
					// Pinch/head direction range.
					u8_transport_mode = TTR_42602_MODE_HD_DIR_SEL;
					// Keep solenoid off for head in forward direction.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_PINCH))
				{
					// Pinch direction selection finished, wait for pinch range.
					u8_transport_mode = TTR_42602_MODE_WAIT_PINCH;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_PINCH_EN))
				{
					// Pinch engage range.
					u8_transport_mode = TTR_42602_MODE_PINCH_SEL;
					// Pull solenoid in to enable pinch roller.
					SOLENOID_ON;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_TAKEUP))
				{
					// Pinch roller engaged, wait for pickup direction range.
					u8_transport_mode = TTR_42602_MODE_WAIT_TAKEUP;
					// No need to release solenoid - next range will be the same, no mechanical bind.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_TAKEUP_DIR))
				{
					// Takeup direction range.
					u8_transport_mode = TTR_42602_MODE_TU_DIR_SEL;
					// Pull solenoid in to select takeup in forward direction.
					SOLENOID_ON;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_MODE)) 
				{
					// Cyclogram finished, waiting for transport to reach stable state.
					u8_transport_mode = TTR_42602_MODE_WAIT_RUN;
					// Release solenoid.
					SOLENOID_OFF;
				}
			}
			else if(u8_target_trr_mode==TTR_42602_MODE_PB_REV)
			{
				// Desired mode: Playback in reverse direction.
				if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_HEAD))
				{
					// Mode change started, wait for pinch direction region.
					u8_transport_mode = TTR_42602_MODE_WAIT_DIR;
					// No need to release solenoid - next range will be the same, no mechanical bind.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_HEAD_DIR))
				{
					// Pinch/head direction range.
					u8_transport_mode = TTR_42602_MODE_HD_DIR_SEL;
					// Keep solenoid in for head in reverse direction.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_PINCH))
				{
					// Pinch direction selection finished, wait for pinch range.
					u8_transport_mode = TTR_42602_MODE_WAIT_PINCH;
					// No need to release solenoid - next range will be the same, no mechanical bind.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_PINCH_EN))
				{
					// Pinch engage range.
					u8_transport_mode = TTR_42602_MODE_PINCH_SEL;
					// Keep solenoid in to enable pinch roller.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_TAKEUP))
				{
					// Pinch roller engaged, wait for pickup direction range.
					u8_transport_mode = TTR_42602_MODE_WAIT_TAKEUP;
					// Release solenoid.
					SOLENOID_OFF;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_TAKEUP_DIR))
				{
					// Takeup direction range.
					u8_transport_mode = TTR_42602_MODE_TU_DIR_SEL;
					// Keep solenoid off for takeup in reverse direction
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_MODE))
				{
					// Cyclogram finished, waiting for transport to reach stable state.
					u8_transport_mode = TTR_42602_MODE_WAIT_RUN;
					// Keep solenoid off.
				}
			}
			else if(u8_target_trr_mode==TTR_42602_MODE_FW_FWD)
			{
				// Desired mode: fast winding in forward direction, head scan in forward direction.
				if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_HEAD))
				{
					// Mode change started, wait for pinch direction region.
					u8_transport_mode = TTR_42602_MODE_WAIT_DIR;
					// Release solenoid.
					SOLENOID_OFF;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_HEAD_DIR))
				{
					// Pinch/head direction range.
					u8_transport_mode = TTR_42602_MODE_HD_DIR_SEL;
					// Keep solenoid off for head in forward direction.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_PINCH))
				{
					// Pinch direction selection finished, wait for pinch range.
					u8_transport_mode = TTR_42602_MODE_WAIT_PINCH;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_PINCH_EN))
				{
					// Pinch engage range.
					u8_transport_mode = TTR_42602_MODE_PINCH_SEL;
					// Keep solenoid off to select fast winding.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_TAKEUP))
				{
					// Pinch roller engaged, wait for pickup direction range.
					u8_transport_mode = TTR_42602_MODE_WAIT_TAKEUP;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_TAKEUP_DIR))
				{
					// Takeup direction range.
					u8_transport_mode = TTR_42602_MODE_TU_DIR_SEL;
					// Pull solenoid in to select takeup in forward direction.
					SOLENOID_ON;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_MODE))
				{
					// Cyclogram finished, waiting for transport to reach stable state.
					u8_transport_mode = TTR_42602_MODE_WAIT_RUN;
					// Release solenoid.
					SOLENOID_OFF;
				}
			}
			else if(u8_target_trr_mode==TTR_42602_MODE_FW_REV)
			{
				// Desired mode: fast winding in reverse direction, head scan in forward direction.
				if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_HEAD))
				{
					// Mode change started, wait for pinch direction region.
					u8_transport_mode = TTR_42602_MODE_WAIT_DIR;
					// Release solenoid.
					SOLENOID_OFF;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_HEAD_DIR))
				{
					// Pinch/head direction range.
					u8_transport_mode = TTR_42602_MODE_HD_DIR_SEL;
					// Keep solenoid off for head in forward direction.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_PINCH))
				{
					// Pinch direction selection finished, wait for pinch range.
					u8_transport_mode = TTR_42602_MODE_WAIT_PINCH;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_PINCH_EN))
				{
					// Pinch engage range.
					u8_transport_mode = TTR_42602_MODE_PINCH_SEL;
					// Keep solenoid off to select fast winding.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_TAKEUP))
				{
					// Pinch roller engaged, wait for pickup direction range.
					u8_transport_mode = TTR_42602_MODE_WAIT_TAKEUP;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_TAKEUP_DIR))
				{
					// Takeup direction range.
					u8_transport_mode = TTR_42602_MODE_TU_DIR_SEL;
					// Keep solenoid off for takeup in reverse direction
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_MODE))
				{
					// Cyclogram finished, waiting for transport to reach stable state.
					u8_transport_mode = TTR_42602_MODE_WAIT_RUN;
					// Keep solenoid off.
				}
			}
			else if(u8_target_trr_mode==TTR_42602_MODE_FW_FWD_HD_REV)
			{
				// Desired mode: fast winding in forward direction, head scan in reverse direction.
				if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_HEAD))
				{
					// Mode change started, wait for pinch direction region.
					u8_transport_mode = TTR_42602_MODE_WAIT_DIR;
					// No need to release solenoid - next range will be the same, no mechanical bind.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_HEAD_DIR))
				{
					// Pinch/head direction range.
					u8_transport_mode = TTR_42602_MODE_HD_DIR_SEL;
					// Keep solenoid in for head in reverse direction.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_PINCH))
				{
					// Pinch direction selection finished, wait for pinch range.
					u8_transport_mode = TTR_42602_MODE_WAIT_PINCH;
					// Release solenoid.
					SOLENOID_OFF;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_PINCH_EN))
				{
					// Pinch engage range.
					u8_transport_mode = TTR_42602_MODE_PINCH_SEL;
					// Keep solenoid off to select fast winding.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_TAKEUP))
				{
					// Pinch roller engaged, wait for pickup direction range.
					u8_transport_mode = TTR_42602_MODE_WAIT_TAKEUP;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_TAKEUP_DIR))
				{
					// Takeup direction range.
					u8_transport_mode = TTR_42602_MODE_TU_DIR_SEL;
					// Pull solenoid in to select takeup in forward direction.
					SOLENOID_ON;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_MODE))
				{
					// Cyclogram finished, waiting for transport to reach stable state.
					u8_transport_mode = TTR_42602_MODE_WAIT_RUN;
					// Release solenoid.
					SOLENOID_OFF;
				}
			}
			else if(u8_target_trr_mode==TTR_42602_MODE_FW_REV_HD_REV)
			{
				// Desired mode: fast winding in reverse direction, head scan in reverse direction.
				if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_HEAD))
				{
					// Mode change started, wait for pinch direction region.
					u8_transport_mode = TTR_42602_MODE_WAIT_DIR;
					// No need to release solenoid - next range will be the same, no mechanical bind.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_HEAD_DIR))
				{
					// Pinch/head direction range.
					u8_transport_mode = TTR_42602_MODE_HD_DIR_SEL;
					// Keep solenoid in for head in reverse direction.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_PINCH))
				{
					// Pinch direction selection finished, wait for pinch range.
					u8_transport_mode = TTR_42602_MODE_WAIT_PINCH;
					// Release solenoid.
					SOLENOID_OFF;
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_PINCH_EN))
				{
					// Pinch engage range.
					u8_transport_mode = TTR_42602_MODE_PINCH_SEL;
					// Keep solenoid off to select fast winding.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_TAKEUP))
				{
					// Pinch roller engaged, wait for pickup direction range.
					u8_transport_mode = TTR_42602_MODE_WAIT_TAKEUP;
					// Keep solenoid off.
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_TAKEUP_DIR))
				{
					// Takeup direction range.
					u8_transport_mode = TTR_42602_MODE_TU_DIR_SEL;
					// Keep solenoid off for takeup in reverse direction
				}
				else if(u8_cycle_timer==(TTR_42602_DELAY_RUN-TIM_42602_DLY_WAIT_MODE))
				{
					// Cyclogram finished, waiting for transport to reach stable state.
					u8_transport_mode = TTR_42602_MODE_WAIT_RUN;
					// Keep solenoid off.
				}
			}
		}
	}
	// Debug PWM.
	//OCR1AL = (u8_transport_mode+1)*(255/TTR_MODE_MAX);
	/*if((u8_cycle_timer>0)||(u8_user_mode!=u8_target_trr_mode)||(u8_target_trr_mode!=u8_transport_mode))
	{
		sprintf(u8a_buf, "MODE|>%03u<|%01u>%01u>%01u\n\r",
				u8_cycle_timer, u8_user_mode, u8_target_trr_mode, u8_transport_mode);
		UART_add_string(u8a_buf);
	}*/
	UART_dump_out();
}

//-------------------------------------- Compare user mode to transport target mode.
uint8_t tanashin_user_change()
{
	if(u8_user_mode==USR_MODE_STOP)
	{
		if(u8_target_trr_mode==TTR_TANA_MODE_STOP)
		{
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	else if(u8_user_mode==USR_MODE_PLAY_FWD)
	{
		if(u8_target_trr_mode==TTR_TANA_MODE_PB_FWD)
		{
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	else if(u8_user_mode==USR_MODE_FWIND_FWD)
	{
		if(u8_target_trr_mode==TTR_TANA_MODE_FW_FWD)
		{
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	else if(u8_user_mode==USR_MODE_FWIND_REV)
	{
		if(u8_target_trr_mode==TTR_TANA_MODE_FW_REV)
		{
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	else
	{
		return FALSE;
	}
}

//-------------------------------------- Perform tape transport state machine for Tanashin-clone tape mech.
void mech_tanashin_state_machine(void)
{
	/*sprintf(u8a_buf, "MODE|>%03u<|%01u>%01u>%01u|%02x\n\r",
	u8_cycle_timer, u8_user_mode, u8_target_trr_mode, u8_transport_mode, sw_state);
	UART_add_string(u8a_buf);*/
	// This transport supports only forward playback.
	u8_last_play_dir = PB_DIR_FWD;
	
	if(u8_cycle_timer==0)
	{
		// Transport is in stable state (mode transition finished).
		// Check if transport is in error-state.
		if(u8_transport_mode==TTR_TANA_MODE_HALT)
		{
			// Transport control is halted due to an error, ignore any state transitions and user-requests.
			// Set upper levels to the same mode.
			u8_target_trr_mode = TTR_TANA_MODE_HALT;
			u8_user_mode = USR_MODE_STOP;
			// Keep timer reset, no mode transitions.
			u8_cycle_timer = 0;
			// Keep capstan stopped.
			CAPSTAN_OFF;
			// Keep solenoid inactive.
			SOLENOID_OFF;
			// Keep service motor stopped.
			SMTR_DIR1_OFF;
			SMTR_DIR2_OFF;
			// Check mechanical stop sensor.
			if((sw_state&TTR_SW_STOP)==0)
			{
				// Transport is not in STOP mode.
				UART_add_flash_string((uint8_t *)cch_halt_force_stop);
				// Force STOP if transport is not in STOP.
				u8_cycle_timer = TIM_TANA_DELAY_STOP;				// Load maximum delay to allow mechanism to revert to STOP (before retrying)
				u8_target_trr_mode = TTR_TANA_MODE_TO_HALT;			// Set mode to trigger solenoid
			}
		}
		// Check if transport has to start transitioning to another stable state.
		else if(u8_target_trr_mode!=u8_transport_mode)
		{
			// Target mode is not the same as current transport mode (need to start transition to another mode).
			UART_add_string("TARGET!=CURRENT : "); UART_dump_Tanashin_mode(u8_target_trr_mode); UART_add_string(" != "); UART_dump_Tanashin_mode(u8_transport_mode); UART_add_flash_string((uint8_t *)cch_endl);
			if(u8_target_trr_mode==TTR_TANA_MODE_TO_INIT)
			{
				// Target mode: start-up delay.
				UART_add_flash_string((uint8_t *)cch_startup_delay);
				u8_cycle_timer = TIM_TANA_DELAY_STOP;				// Load maximum delay to allow mechanism to stabilize
				u8_transport_mode = TTR_TANA_MODE_INIT;				// Set mode for waiting initialization
				u8_target_trr_mode = TTR_TANA_MODE_STOP;			// Set next mode to STOP
				u8_user_mode = USR_MODE_STOP;						// Reset user mode to STOP
			}
			else if(u8_target_trr_mode==TTR_TANA_MODE_STOP)
			{
				// Target mode: full stop.
				UART_add_string("New: stop\n\r");
				u8_cycle_timer = TIM_TANA_DELAY_STOP;
				u8_transport_mode = TTR_TANA_MODE_TO_STOP;
			}
			else
			{
				// Reset last error.
				u8_transport_error = TTR_ERR_NONE;
				// Reset tachometer timer.
				u8_tacho_timer = 0;
				// Check new target mode.
				// TODO: select transition scheme from current mode & target mode.
				if((u8_target_trr_mode==TTR_TANA_MODE_PB_FWD)
					||(u8_target_trr_mode==TTR_TANA_MODE_FW_FWD)||(u8_target_trr_mode==TTR_TANA_MODE_FW_REV))
				{
					// Start transition to active mode.
					UART_add_string("New: active\n\r");
					u8_cycle_timer = TTR_42602_DELAY_RUN;
					u8_transport_mode = TTR_42602_MODE_TO_START;
				}
				else
				{
					// Unknown mode, reset to STOP.
					UART_add_flash_string((uint8_t *)cch_unknown_mode);
					u8_target_trr_mode = TTR_TANA_MODE_STOP;
					u8_user_mode = USR_MODE_STOP;
				}
			}
		}
		// Transport is not in error and is in stable state (target mode reached),
		// check if user requests another mode.
		else if(tanashin_user_change()==TRUE)
		{
			// Not in disabled state, target mode is reached.
			// User wants another mode than current transport target is.
			UART_add_string("USER!=TARGET : "); UART_dump_Tanashin_mode(u8_user_mode); UART_add_string(" != "); UART_dump_Tanashin_mode(u8_target_trr_mode); UART_add_flash_string((uint8_t *)cch_endl);
			// TODO: switching between modes.
			/*if(u8_target_trr_mode!=TTR_42602_MODE_STOP)
			{
				// Mechanism is in active mode, user wants another mode, set target as STOP, it's the only way to transition to another mode. 
				// New target mode will apply in the next run of the [transport_state_machine()].
				u8_target_trr_mode = TTR_42602_MODE_STOP;
				UART_add_string("New: stop (u)\n\r");
			}
			else
			{
				// Mechanism is in STOP mode, simple: set target to what user wants.
				// New target mode will apply in the next run of the [transport_state_machine()].
				u8_target_trr_mode = u8_user_mode;
				UART_add_string("New: active (u)\n\r");
			}*/
		}
		else
		{
			// Transport is not due to transition through modes (u8_transport_mode == u8_target_trr_mode == u8_user_mode).
			if(u8_transport_mode==TTR_TANA_MODE_STOP)
			{
				// Transport supposed to be in STOP.
				// Check mechanism for mechanical STOP condition.
				if((sw_state&TTR_SW_STOP)==0)
				{
					// Transport is not in STOP mode.
					UART_add_flash_string((uint8_t *)cch_stop_active);
					u8_cycle_timer = TIM_TANA_DELAY_STOP;
					// Force STOP if transport is not in STOP.
					u8_cycle_timer = TIM_TANA_DELAY_STOP;				// Load maximum delay to allow mechanism to revert to STOP (before retrying)
					u8_transport_mode = TIM_TANA_DLY_WAIT_REW_ACT;		// Set mode to trigger solenoid
					u8_target_trr_mode = TTR_TANA_MODE_STOP;			// Set target to be STOP
				}
			}
			else if(u8_transport_mode==TTR_TANA_MODE_PB_FWD)
			{
				// Transport supposed to be in PLAYBACK.
				// Check tachometer timer.
				if(u8_tacho_timer>TACHO_TANA_PLAY_DLY_MAX)
				{
					// No signal from takeup tachometer for too long.
					// Perform auto-stop.
					u8_target_trr_mode = TTR_TANA_MODE_STOP;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
					UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
				}
				// Check if tape has dropped out.
				else if((sw_state&TTR_SW_TAPE_IN)==0)
				{
					if((u8_target_trr_mode!=TTR_TANA_MODE_STOP)||(u8_user_mode!=USR_MODE_STOP))
					{
						UART_add_flash_string((uint8_t *)cch_no_tape);
					}
					// Tape is out, clear any active mode.
					u8_target_trr_mode = TTR_TANA_MODE_STOP;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
				}
				// Check if somehow (manually?) transport switched into STOP.
				else if((sw_state&TTR_SW_STOP)!=0)
				{
					// Mechanism unexpectedly slipped into STOP.
					UART_add_flash_string((uint8_t *)cch_stop_corr);
					// Correct logic mode.
					u8_transport_mode = TTR_TANA_MODE_STOP;
					u8_target_trr_mode = TTR_TANA_MODE_STOP;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
				}
			}
			else if((u8_transport_mode==TTR_TANA_MODE_FW_FWD)||(u8_transport_mode==TTR_TANA_MODE_FW_REV))
			{
				// Transport supposed to be in FAST WIND.
				// Check tachometer timer.
				if(u8_tacho_timer>TACHO_TANA_FWIND_DLY_MAX)
				{
					// No signal from takeup tachometer for too long.
					// Perform auto-stop.
					u8_target_trr_mode = TTR_TANA_MODE_STOP;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
					UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
				}
				// Check if tape has dropped out.
				else if((sw_state&TTR_SW_TAPE_IN)==0)
				{
					if((u8_target_trr_mode!=TTR_TANA_MODE_STOP)||(u8_user_mode!=USR_MODE_STOP))
					{
						UART_add_flash_string((uint8_t *)cch_no_tape);
					}
					// Tape is out, clear any active mode.
					u8_target_trr_mode = TTR_TANA_MODE_STOP;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
				}
				// Check if somehow (manually?) transport switched into STOP.
				else if((sw_state&TTR_SW_STOP)!=0)
				{
					// Mechanism unexpectedly slipped into STOP.
					UART_add_flash_string((uint8_t *)cch_stop_corr);
					// Correct logic mode.
					u8_transport_mode = TTR_TANA_MODE_STOP;
					u8_target_trr_mode = TTR_TANA_MODE_STOP;
					// Clear user mode.
					u8_user_mode = USR_MODE_STOP;
				}
			}
		}
	}
	else
	{
		// Transport in transitioning through states.
		// Count down mode transition timer.
		u8_cycle_timer--;
		// Check if transition is finished.
		if(u8_cycle_timer==0)
		{
			// Desired mode reached.
			// De-energize solenoid.
			SOLENOID_OFF;
			// Check if transport is not in error.
			if(u8_transport_mode!=TTR_TANA_MODE_HALT)
			{
				//sprintf(u8a_buf, "TRFIN|%01u>%01u\n\r", u8_transport_mode, u8_target_trr_mode);
				UART_add_string("Mode transition done: "); UART_dump_Tanashin_mode(u8_transport_mode); UART_add_string(" > "); UART_dump_Tanashin_mode(u8_target_trr_mode); UART_add_flash_string((uint8_t *)cch_endl);
				
				// Save reached state.
				u8_transport_mode = u8_target_trr_mode;
				// Check if mechanism successfully reached target logic state.
				// Check if target was one of the active modes.
				if((u8_target_trr_mode==TTR_TANA_MODE_PB_FWD)
					||(u8_target_trr_mode==TTR_TANA_MODE_FW_FWD)||(u8_target_trr_mode==TTR_TANA_MODE_FW_REV))
				{
					// Check if mechanical STOP state wasn't cleared.
					if((sw_state&TTR_SW_STOP)!=0)
					{
						// Mechanically mode didn't change from STOP, register an error.
						u8_transport_mode = TTR_TANA_MODE_HALT;
						u8_transport_error += TTR_ERR_NO_CTRL;
						//UART_add_string("Reached active but STOP is present, halting...\n\r");
						UART_add_flash_string((uint8_t *)cch_halt_stop1);
						UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
					}
				}
			}
			// Reset tachometer timer;
			u8_tacho_timer = 0;
		}
		else
		{
			// Still in transition (counter != 0).
			// TODO
			if((u8_target_trr_mode==TTR_TANA_MODE_TO_STOP)||(u8_target_trr_mode==TTR_TANA_MODE_TO_HALT))
			{
				if(u8_cycle_timer>(TIM_TANA_DELAY_STOP-TIM_TANA_DLY_SW_ACT))
				{
					// Energize solenoid.
					SOLENOID_ON;
				}
				else
				{
					// De-energize solenoid.
					SOLENOID_OFF;
				}
			}
			else if(u8_target_trr_mode==TTR_TANA_MODE_PB_FWD)
			{
				if(u8_cycle_timer>(TIM_TANA_DLY_PB_WAIT-TIM_TANA_DLY_SW_ACT))
				{
					// Energize solenoid.
					SOLENOID_ON;
				}
				else
				{
					// De-energize solenoid.
					SOLENOID_OFF;
				}
			}
			else if(u8_target_trr_mode==TTR_TANA_MODE_FW_FWD)
			{
				
			}
			else if(u8_target_trr_mode==TTR_TANA_MODE_FW_REV)
			{
				
			}
		}
	}
	// Debug output.
	//OCR1AL = (u8_transport_mode+1)*(255/TTR_MODE_MAX);
	if((u8_cycle_timer>0)||(u8_user_mode!=u8_target_trr_mode)||(u8_target_trr_mode!=u8_transport_mode))
	{
		sprintf(u8a_buf, "MODE|>%03u<|%01u>%01u>%01u|%02x\n\r",
				u8_cycle_timer, u8_user_mode, u8_target_trr_mode, u8_transport_mode, sw_state);
		UART_add_string(u8a_buf);
	}
	UART_dump_out();
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
			//u8_cycle_timer = TIM_TANASHIN_DLY_PB_WAIT;
			u8_cycle_timer = 255;
			SOLENOID_ON;
		}*/
		if(u8_user_mode==USR_MODE_PLAY_FWD)
		{
			// Preset time for full cycle from STOP to PLAY.
			u8_cycle_timer = TIM_TANA_DLY_PB_WAIT;
		}
		else if(u8_user_mode==USR_MODE_FWIND_REV)
		{
			// Preset time for full cycle from PLAY to FAST WIND.
			u8_cycle_timer = TIM_TANA_DLY_FWIND_WAIT;
		}
		else if(u8_user_mode==USR_MODE_FWIND_FWD)
		{
			// Preset time for full cycle from PLAY to FAST WIND.
			u8_cycle_timer = TIM_TANA_DLY_FWIND_WAIT;
		}
		u8_user_mode = USR_MODE_STOP;
	}
	
	if(u8_cycle_timer>0)
	{
		uint8_t sol_state;
		sol_state = 0;
		if(SOLENOID_STATE!=0) sol_state = 1;
		sprintf(u8a_buf, "MODE|>%03u<|%02x|%01x\n\r", u8_cycle_timer, sw_state, sol_state);
		UART_add_string(u8a_buf);

		if(u8_user_mode==USR_MODE_PLAY_FWD)
		{
			if(u8_cycle_timer>(TIM_TANA_DLY_PB_WAIT-TIM_TANA_DLY_SW_ACT))
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
			if(u8_cycle_timer>(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_SW_ACT))
			{
				// Enable solenoid to start gear rotation.
				SOLENOID_ON;
			}
			else if(u8_cycle_timer>(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_WAIT_REW_ACT))
			{
				// Disable solenoid, wait for the decision making point for fast wind direction.
				SOLENOID_OFF;
			}
			else if((u8_cycle_timer>(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_WAIT_REW_ACT-TIM_TANA_DLY_SW_ACT))&&(u8_user_mode==USR_MODE_FWIND_REV))
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
		u8_cycle_timer--;
		/*if(u8_cycle_timer<(u8_dbg_timer-TIM_TANASHIN_DLY_SW_ACT))
		{
			SOLENOID_OFF;
		}
		else if(u8_cycle_timer<u8_dbg_timer)
		{
			SOLENOID_ON;
		}
		else if(u8_cycle_timer<(255-TIM_TANASHIN_DLY_SW_ACT))
		{
			SOLENOID_OFF;
		}*/
		if(u8_cycle_timer==0)
		{
			SOLENOID_OFF;
			u8_transport_mode = TTR_42602_MODE_STOP;
			u8_user_mode = USR_MODE_STOP;
		}
		
	}
	UART_dump_out();
}

inline void UART_dump_settings(void)
{
	if((u8_reverse&TTR_REV_REW_AUTO)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_rewind); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_rewind); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((u8_reverse&TTR_REV_ENABLE)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_reverse); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_reverse); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((u8_reverse&TTR_REV_PB_AUTO)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_ab); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_ab); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((u8_reverse&TTR_REV_PB_LOOP)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_loop); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_loop); UART_add_flash_string((uint8_t *)cch_disabled);
	}
}

//-------------------------------------- Main function.
int main(void)
{
	// Start-up initialization.
	system_startup();
	
	// Default mech.
	u8_mech_type = TTR_TYPE_TANASHIN;
	
	// Init modes to selected transport.
	if(u8_mech_type==TTR_TYPE_CRP42602Y)
	{
		u8_user_mode = USR_MODE_STOP;
		u8_target_trr_mode = TTR_42602_MODE_TO_INIT;
		u8_transport_mode = TTR_42602_MODE_STOP;
	}
	else if(u8_mech_type==TTR_TYPE_TANASHIN)
	{
		u8_user_mode = USR_MODE_STOP;
		u8_target_trr_mode = TTR_TANA_MODE_TO_INIT;
		u8_transport_mode = TTR_TANA_MODE_STOP;
		u8_reverse &= ~(TTR_REV_ENABLE|TTR_REV_PB_AUTO|TTR_REV_PB_LOOP);	// Disable reverse functions for non-reverse mech.
	}
	
	// Output startup messages.
	UART_add_flash_string((uint8_t *)cch_startup_1);
	UART_add_flash_string((uint8_t *)ucaf_info); UART_add_flash_string((uint8_t *)cch_endl); 
	UART_add_flash_string((uint8_t *)ucaf_version); UART_add_string(" ["); UART_add_flash_string((uint8_t *)ucaf_compile_date); UART_add_string(", "); UART_add_flash_string((uint8_t *)ucaf_compile_time); UART_add_string("]"); UART_add_flash_string((uint8_t *)cch_endl);
	UART_add_flash_string((uint8_t *)ucaf_author); UART_add_flash_string((uint8_t *)cch_endl); UART_dump_out();
	UART_add_flash_string((uint8_t *)cch_endl); UART_dump_settings(); UART_add_flash_string((uint8_t *)cch_endl); 
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
				// Update LEDs.
				update_indicators();
				// Update transport state machine and solenoid action.
				if(u8_mech_type==TTR_TYPE_CRP42602Y)
				{
					mech_crp42602y_state_machine();
				}
				else if(u8_mech_type==TTR_TYPE_TANASHIN)
				{
					mech_tanashin_state_machine();
				}
				else
				{
					mech_log();
				}
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

