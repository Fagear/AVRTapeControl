#include "mech_tanashin.h"

uint8_t u8_tanashin_target_mode=TTR_TANA_MODE_TO_INIT;	// Target transport mode (derived from [usr_mode])
uint8_t u8_tanashin_mode=TTR_TANA_MODE_STOP;			// Current tape transport mode (transitions to [u8_tanashin_target_mode])
uint8_t u8_tanashin_error=TTR_ERR_NONE;					// Last transport error
uint8_t u8_tanashin_trans_timer=0;						// Solenoid holding timer
uint16_t u16_tanashin_idle_time=0;						// Timer for disabling capstan motor
uint8_t u8_tanashin_retries=0;							// Number of retries before transport halts

#ifdef UART_TERM
char u8a_tanashin_buf[24];								// Buffer for UART debug messages
#endif /* UART_TERM */

#ifdef SUPP_TANASHIN_MECH
volatile const uint8_t ucaf_tanashin_mech[] PROGMEM = "Tanashin-clone mechanism";
#endif /* SUPP_TANASHIN_MECH */

//-------------------------------------- Freeze transport due to error.
void mech_tanashin_set_error(uint8_t in_err)
{
	u8_tanashin_target_mode = TTR_TANA_MODE_HALT;
	u8_tanashin_mode = TTR_TANA_MODE_HALT;
	u8_tanashin_error += in_err;	
}

//-------------------------------------- Convert user mode to transport mode.
uint8_t mech_tanashin_user_to_transport(uint8_t in_mode)
{
	if(in_mode==USR_MODE_PLAY_FWD)
	{
		return TTR_TANA_MODE_PB_FWD;
	}
	if(in_mode==USR_MODE_REC_FWD)
	{
		return TTR_TANA_MODE_RC_FWD;
	}
	if(in_mode==USR_MODE_FWIND_FWD)
	{
		return TTR_TANA_MODE_FW_FWD;
	}
	if(in_mode==USR_MODE_FWIND_REV)
	{
		return TTR_TANA_MODE_FW_REV;
	}
	return TTR_TANA_MODE_STOP;
}

//-------------------------------------- Transport operations are halted, keep mechanism in this state.
void mech_tanashin_static_halt(uint8_t in_sws, uint8_t *usr_mode)
{
	// Set upper levels to the same mode.
	u8_tanashin_target_mode = TTR_TANA_MODE_HALT;
	// Clear user mode.
	(*usr_mode) = USR_MODE_STOP;
	// Turn on mute.
	MUTE_EN_ON;
	// Turn off recording circuit.
	REC_EN_OFF;
	// Keep timer reset, no mode transitions.
	u8_tanashin_trans_timer = 0;
	// Check mechanical stop sensor.
	if((in_sws&TTR_SW_STOP)==0)
	{
		// Transport is not in STOP mode.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_halt_active); UART_add_flash_string((uint8_t *)cch_force_stop);
#endif /* UART_TERM */
		// Start capstan motor.
		CAPSTAN_ON;
		// Force STOP if transport is not in STOP.
		u8_tanashin_trans_timer = TIM_TANA_DLY_ACTIVE;				// Load maximum delay to allow mechanism to revert to STOP (before retrying)
		u8_tanashin_mode = TTR_TANA_SUBMODE_TO_HALT;				// Set mode to trigger solenoid
	}
	else
	{
		// Keep solenoid inactive.
		SOLENOID_OFF;
		// Disable power for tacho sensor.
		TANA_TACHO_PWR_DIS;
		// Shut down capstan motor.
		CAPSTAN_OFF;
	}
}

//-------------------------------------- Start transition from current mode to target mode.
void mech_tanashin_target2mode(uint8_t *tacho, uint8_t *usr_mode)
{
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_target2current1); mech_tanashin_UART_dump_mode(u8_tanashin_mode);
	UART_add_flash_string((uint8_t *)cch_target2current2); mech_tanashin_UART_dump_mode(u8_tanashin_target_mode); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
	// Before any transition...
	// Turn on mute.
	MUTE_EN_ON;
	// Turn off recording circuit.
	REC_EN_OFF;
	// For any mode transition capstan must be running.
	// Check if capstan is running.
	if(CAPSTAN_STATE==0)
	{
		// Capstan is stopped.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_capst_start);
#endif /* UART_TERM */
		// Override target mode to perform capstan spinup wait.
		u8_tanashin_target_mode = TTR_TANA_MODE_TO_INIT;
	}
	// Reset idle timer.
	u16_tanashin_idle_time = 0;
	// Reset tachometer timeout.
	(*tacho) = 0;
	if(u8_tanashin_target_mode==TTR_TANA_MODE_TO_INIT)
	{
		// Target mode: start-up delay.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_startup_delay);
#endif /* UART_TERM */
		// Set time for waiting for mechanism to stabilize.
		u8_tanashin_trans_timer = TIM_TANA_DLY_ACTIVE;
		// Put transport in init waiting mode.
		u8_tanashin_mode = TTR_TANA_SUBMODE_INIT;
		// Move target to STOP mode.
		u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
	}
	else if(u8_tanashin_target_mode==TTR_TANA_MODE_STOP)
	{
		// Target mode: full STOP.
		if((u8_tanashin_mode==TTR_TANA_MODE_PB_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_RC_FWD))
		{
			// From playback/record there only way is to fast wind, need to get through that.
			u8_tanashin_trans_timer = TIM_TANA_DLY_PB2STOP;
			u8_tanashin_mode = TTR_TANA_SUBMODE_TO_SKIP_FW;
		}
		else if((u8_tanashin_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_FW_REV))
		{
			// Next from fast wind is STOP.
			u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
			u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
		}
		else
		{
			// TTR is in unknown state.
			mech_tanashin_set_error(TTR_ERR_LOGIC_FAULT);
		}
	}
	else if(u8_tanashin_target_mode==TTR_TANA_MODE_HALT)
	{
		// Target mode: full stop in HALT.
		u8_tanashin_mode = TTR_TANA_MODE_HALT;
	}
	else
	{
		// Check new target mode.
		if((u8_tanashin_target_mode==TTR_TANA_MODE_PB_FWD)||(u8_tanashin_target_mode==TTR_TANA_MODE_RC_FWD))
		{
			// Target mode: PLAYBACK/RECORD.
			if(u8_tanashin_mode==TTR_TANA_MODE_STOP)
			{
				// Playback/record can be selected only from STOP.
				u8_tanashin_trans_timer = TIM_TANA_DLY_PB_WAIT;
				u8_tanashin_mode = TTR_TANA_SUBMODE_TO_PLAY;
			}
			else if((u8_tanashin_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_FW_REV))
			{
				// From fast wind the mode has to become STOP at first.
				u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
				u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
			}
			else
			{
				// TTR is in unknown state.
				mech_tanashin_set_error(TTR_ERR_LOGIC_FAULT);
			}
		}
		else if((u8_tanashin_target_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_target_mode==TTR_TANA_MODE_FW_REV))
		{
			// Target mode: FAST WIND.
			if(u8_tanashin_mode==TTR_TANA_MODE_STOP)
			{
				// Fast wind can be selected only through PLAY.
				u8_tanashin_trans_timer = TIM_TANA_DLY_PB_WAIT;
				u8_tanashin_mode = TTR_TANA_SUBMODE_TO_PLAY;
			}
			else if((u8_tanashin_mode==TTR_TANA_MODE_PB_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_RC_FWD))
			{
				// Direct select FAST WIND from PLAY/RECORD.
				u8_tanashin_trans_timer = TIM_TANA_DLY_FWIND_WAIT;
				u8_tanashin_mode = TTR_TANA_SUBMODE_TO_FWIND;
			}
			else if((u8_tanashin_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_FW_REV))
			{
				// Re-select FAST WIND through STOP.
				u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
				u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
			}
			else
			{
				// TTR is in unknown state.
				mech_tanashin_set_error(TTR_ERR_LOGIC_FAULT);
			}
		}
		else
		{
			// Unknown mode, reset to STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_unknown_mode);
#endif /* UART_TERM */
			u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
			(*usr_mode) = USR_MODE_STOP;
		}
		// Reset last error.
		u8_tanashin_error = TTR_ERR_NONE;
	}
}

//-------------------------------------- Take in user desired mode and set new target mode.
void mech_tanashin_user2target(uint8_t *usr_mode)
{
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_user2target1); mech_tanashin_UART_dump_mode(u8_tanashin_target_mode);
	UART_add_flash_string((uint8_t *)cch_user2target2); UART_dump_user_mode((*usr_mode)); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
	// New target mode will apply in the next run of the [mech_tanashin_state_machine()].
	u8_tanashin_target_mode = mech_tanashin_user_to_transport((*usr_mode));
}

//-------------------------------------- Control mechanism in static mode (not transitioning between modes).
void mech_tanashin_static_mode(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode)
{
	if(u8_tanashin_mode==TTR_TANA_MODE_STOP)
	{
		// Transport supposed to be in STOP.
		// Increase idle timer.
		if(u16_tanashin_idle_time<IDLE_CAP_MAX)
		{
			u16_tanashin_idle_time++;
		}
		// Keep mute on.
		MUTE_EN_ON;
		// Keep recording circuit off.
		REC_EN_OFF;
		// Check mechanism for mechanical STOP condition.
		if((in_sws&TTR_SW_STOP)==0)
		{
			// Transport is not in STOP mode.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_stop_active); UART_add_flash_string((uint8_t *)cch_force_stop);
#endif /* UART_TERM */
			// Force STOP if transport is not in STOP.
			u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;			// Load maximum delay to allow mechanism to revert to STOP (before retrying)
			u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;			// Set mode to trigger solenoid
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;			// Set target to be STOP
		}
	}
	else if((u8_tanashin_mode==TTR_TANA_MODE_PB_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_RC_FWD))
	{
		// Transport supposed to be in PLAYBACK or RECORD.
		// Reset idle timer.
		u16_tanashin_idle_time = 0;
		// Check tachometer timer.
		if((*tacho)>TACHO_TANA_PLAY_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
			// Turn mute on.
			MUTE_EN_ON;
			// Turn recording circuit off.
			REC_EN_OFF;
			// Perform auto-stop.
			u8_tanashin_trans_timer = TIM_TANA_DLY_PB2STOP;
			u8_tanashin_mode = TTR_TANA_SUBMODE_TO_SKIP_FW;
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
			if((in_features&TTR_FEA_PBF2REW)!=0)
			{
				// Currently: playback or recording in forward, auto-rewind is enabled.
				// Next: rewind.
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_rewind);
#endif /* UART_TERM */
				// Queue rewind.
				(*usr_mode) = USR_MODE_FWIND_REV;
			}
#ifdef UART_TERM
			else
			{
				// Currently: playback or recording in forward, auto-rewind is disabled.
				// Next: stop.
				UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
				// STOP mode already queued.
			}
#endif /* UART_TERM */
		}
		else
		{
			// No tachometer timeout.
			// Keep mute off.
			MUTE_EN_OFF;
			if(u8_tanashin_mode==TTR_TANA_MODE_RC_FWD)
			{
				// Keep recording circuit on.
				REC_EN_ON;
			}
		}
		// Check if somehow (manually?) transport switched into STOP.
		if((in_sws&TTR_SW_STOP)!=0)
		{
			// Mechanism unexpectedly slipped into STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_stop_corr);
#endif /* UART_TERM */
			// Correct logic mode.
			u8_tanashin_mode = TTR_TANA_MODE_STOP;
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
	else if((u8_tanashin_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_FW_REV))
	{
		// Transport supposed to be in FAST WIND.
		// Keep mute on.
		MUTE_EN_ON;
		// Keep recording circuit off.
		REC_EN_OFF;
		// Reset idle timer.
		u16_tanashin_idle_time = 0;
		// Check tachometer timer.
		if((*tacho)>TACHO_TANA_FWIND_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
			// Perform auto-stop.
			u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
			
			if(u8_tanashin_mode==TTR_TANA_MODE_FW_FWD)
			{
				// Fast wind was in forward direction.
				if((in_features&TTR_FEA_FF2REW)!=0)
				{
					// Currently: fast wind in forward direction, auto-rewind is enabled.
					// Next: rewind.
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_rewind);
#endif /* UART_TERM */
					// Queue rewind.
					(*usr_mode) = USR_MODE_FWIND_REV;
				}
#ifdef UART_TERM
				else
				{
					// Currently: fast wind in forward direction, auto-rewind is disabled.
					// Next: stop.
					UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
					// STOP mode already queued.
				}
#endif /* UART_TERM */
			}
#ifdef UART_TERM
			else
			{
				// Currently: fast wind in reverse direction.
				// Next: stop.
				UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
				// STOP mode already queued.
			}
#endif /* UART_TERM */
			u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
		}
		// Check if somehow (manually?) transport switched into STOP.
		if((in_sws&TTR_SW_STOP)!=0)
		{
			// Mechanism unexpectedly slipped into STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_stop_corr);
#endif /* UART_TERM */
			// Correct logic mode.
			u8_tanashin_mode = TTR_TANA_MODE_STOP;
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
}

//-------------------------------------- Transition through modes, timing solenoid.
void mech_tanashin_cyclogram(uint8_t in_sws)
{
	if(u8_tanashin_mode==TTR_TANA_SUBMODE_INIT)
	{
		// Desired mode: spin-up capstan, wait for TTR to stabilize.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Enable power for tacho sensor.
		TANA_TACHO_PWR_EN;
		// Turn off solenoid, let mechanism stabilize.
		SOLENOID_OFF;
		// Turn on mute.
		MUTE_EN_ON;
		// Turn off recording circuit.
		REC_EN_OFF;
		if(u8_tanashin_trans_timer==0)
		{
			// Transition is done.
			// Set stable state.
			u8_tanashin_mode = u8_tanashin_target_mode;
		}
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_TO_STOP)
	{
		// Starting transition to STOP mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Enable power for tacho sensor.
		TANA_TACHO_PWR_EN;
		// Activate solenoid to start transition to STOP.
		SOLENOID_ON;
		if(u8_tanashin_trans_timer<(TIM_TANA_DLY_STOP-TIM_TANA_DLY_SW_ACT))
		{
			// Initial time for activating mode transition to STOP has passed.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Wait for transport to transition to STOP.
			u8_tanashin_mode = TTR_TANA_SUBMODE_WAIT_STOP;
		}
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_WAIT_STOP)
	{
		// Transitioning to STOP mode.
		// Deactivate solenoid.
		SOLENOID_OFF;
		if(u8_tanashin_trans_timer==0)
		{
			// Transition is done.
			// Set stable state.
			u8_tanashin_mode = TTR_TANA_MODE_STOP;
			// Check if mechanical STOP state wasn't reached.
			if((in_sws&TTR_SW_STOP)==0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_stop_active); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_mode_failed);
				sprintf(u8a_tanashin_buf, " %01u\n\r", u8_tanashin_retries);
				UART_add_string(u8a_tanashin_buf);
#endif /* UART_TERM */
				// Increase number of retries before failing.
				u8_tanashin_retries++;
				if(u8_tanashin_retries>=MODE_REP_MAX)
				{
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
					// Mechanically mode didn't change from active, register an error.
					mech_tanashin_set_error(TTR_ERR_NO_CTRL);
				}
				else
				{
					// Repeat transition to STOP.
					u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
					u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
				}
			}
			else
			{
				// Reset retry count.
				u8_tanashin_retries = 0;
			}
		}
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_TO_PLAY)
	{
		// Starting transition to PLAY/RECORD mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Enable power for tacho sensor.
		TANA_TACHO_PWR_EN;
		// Activate solenoid to start transition to PLAY/RECORD.
		SOLENOID_ON;
		if(u8_tanashin_target_mode==TTR_TANA_MODE_RC_FWD)
		{
			// Turn on recording circuit (before heads contact the tape).
			REC_EN_ON;
		}
		u8_tanashin_mode = TTR_TANA_SUBMODE_WAIT_PLAY;
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_WAIT_PLAY)
	{
		// Transitioning to PLAY/RECORD mode.
		if(u8_tanashin_trans_timer==0)
		{
			// Transition is done.
			// Deactivate solenoid.
			SOLENOID_OFF;
			if(u8_tanashin_target_mode==TTR_TANA_MODE_RC_FWD)
			{
				// Set stable RECORD state.
				u8_tanashin_mode = TTR_TANA_MODE_RC_FWD;
			}
			else
			{
				// Set stable PLAY state.
				u8_tanashin_mode = TTR_TANA_MODE_PB_FWD;
			}
			// Check if mechanical STOP state wasn't cleared.
			if((in_sws&TTR_SW_STOP)!=0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_active_stop); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
				// Mechanically mode didn't change from STOP, register an error.
				mech_tanashin_set_error(TTR_ERR_NO_CTRL);
			}
			else
			{
				// Turn off mute.
				MUTE_EN_OFF;
			}
		}
		else if(u8_tanashin_trans_timer<(TIM_TANA_DLY_PB_WAIT-TIM_TANA_DLY_SW_ACT))
		{
			// Deactivate solenoid and wait for transition to happen.
			SOLENOID_OFF;
		}
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_TO_FWIND)
	{
		// Starting transition to FAST WIND mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Enable power for tacho sensor.
		TANA_TACHO_PWR_EN;
		// Activate solenoid to start transition to FAST WIND.
		SOLENOID_ON;
		u8_tanashin_mode = TTR_TANA_SUBMODE_WAIT_FWIND;
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_WAIT_FWIND)
	{
		// Transitioning to FAST WIND mode.
		if(u8_tanashin_trans_timer==0)
		{
			// Transition is done.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Check takeup direction for FAST WIND.
			if(u8_tanashin_target_mode==TTR_TANA_MODE_FW_REV)
			{
				// Set stable state.
				u8_tanashin_mode = TTR_TANA_MODE_FW_REV;
			}
			else
			{
				// Set stable state.
				u8_tanashin_mode = TTR_TANA_MODE_FW_FWD;
			}
			// Check if mechanical STOP state suddenly appeared.
			if((in_sws&TTR_SW_STOP)!=0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_active_stop); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
				// Mechanically mode slipped to STOP, register an error.
				mech_tanashin_set_error(TTR_ERR_NO_CTRL);
			}
		}
		else if(u8_tanashin_trans_timer<(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_FWIND_ACT))
		{
			// Deactivate solenoid and wait for transition to happen.
			SOLENOID_OFF;
		}
		else if(u8_tanashin_trans_timer<(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_WAIT_REW_ACT))
		{
			// Check takeup direction for FAST WIND.
			if(u8_tanashin_target_mode==TTR_TANA_MODE_FW_REV)
			{
				// Activate solenoid for reverse direction.
				SOLENOID_ON;
			}
		}
		else if(u8_tanashin_trans_timer<(TIM_TANA_DLY_FWIND_WAIT-TIM_TANA_DLY_SW_ACT))
		{
			// Deactivate solenoid and wait for takeup selection range.
			SOLENOID_OFF;
		}
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_TO_SKIP_FW)
	{
		// Starting transition to STOP mode skipping FAST WIND mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Enable power for tacho sensor.
		TANA_TACHO_PWR_EN;
		// Activate solenoid to start transition to STOP through FAST WIND.
		SOLENOID_ON;
		u8_tanashin_mode = TTR_TANA_SUBMODE_WAIT_SKIP;
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_WAIT_SKIP)
	{
		// Transitioning to STOP mode through FAST WIND.
		if(u8_tanashin_trans_timer==0)
		{
			// Transition is done.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Set stable state.
			u8_tanashin_mode = TTR_TANA_MODE_STOP;
			// Check if mechanical STOP state wasn't reached.
			if((in_sws&TTR_SW_STOP)==0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_stop_active); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_mode_failed);
				sprintf(u8a_tanashin_buf, " %01u\n\r", u8_tanashin_retries);
				UART_add_string(u8a_tanashin_buf);
#endif /* UART_TERM */
				// Increase number of retries before failing.
				u8_tanashin_retries++;
				if(u8_tanashin_retries>=MODE_REP_MAX)
				{
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
					// Mechanically mode didn't change from active, register an error.
					mech_tanashin_set_error(TTR_ERR_NO_CTRL);
				}
				else
				{
					// Repeat transition to STOP.
					u8_tanashin_trans_timer = TIM_TANA_DLY_STOP;
					u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
				}
			}
		}
		else if(u8_tanashin_trans_timer<(TIM_TANA_DLY_PB2STOP-TIM_TANA_DLY_SKIP_END))
		{
			// Deactivate solenoid and wait for transition to STOP.
			SOLENOID_OFF;
		}
		else if(u8_tanashin_trans_timer<(TIM_TANA_DLY_PB2STOP-TIM_TANA_DLY_FWIND_SKIP))
		{
			// Activate solenoid and wait for skipping FAST WIND.
			SOLENOID_ON;
		}
		else if(u8_tanashin_trans_timer<(TIM_TANA_DLY_PB2STOP-TIM_TANA_DLY_SW_ACT))
		{
			// Deactivate solenoid and wait for takeup selection range.
			SOLENOID_OFF;
		}
	}
	else if(u8_tanashin_mode==TTR_TANA_SUBMODE_TO_HALT)
	{
		// Starting transition to HALT mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Activate solenoid to start transition to HALT.
		SOLENOID_ON;
		u8_tanashin_mode = TTR_TANA_MODE_HALT;
	}
	else if(u8_tanashin_mode==TTR_TANA_MODE_HALT)
	{
		// Desired mode: recovery STOP in HALT mode.
		if(u8_tanashin_trans_timer<(TIM_TANA_DLY_ACTIVE-TIM_TANA_DLY_SW_ACT))
		{
			// Initial time for activating mode transition to STOP has passed.
			// Release solenoid while waiting for transport to transition to STOP.
			SOLENOID_OFF;
		}
	}
	
#ifdef UART_TERM
	if((u8_tanashin_trans_timer==0)&&(u8_tanashin_mode!=TTR_TANA_MODE_HALT))
	{
		UART_add_flash_string((uint8_t *)cch_mode_done); UART_add_flash_string((uint8_t *)cch_arrow);
		mech_tanashin_UART_dump_mode(u8_tanashin_mode); UART_add_flash_string((uint8_t *)cch_endl);
	}
#endif /* UART_TERM */
}

//-------------------------------------- Perform tape transport state machine.
void mech_tanashin_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	// Mode overflow protection.
	if((u8_tanashin_mode>=TTR_TANA_MODE_MAX)||(u8_tanashin_target_mode>=TTR_TANA_MODE_MAX))
	{
		// Register logic error.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_halt_stop3); UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
		mech_tanashin_set_error(TTR_ERR_LOGIC_FAULT);
	}
	// This transport supports only forward playback.
	(*play_dir) = PB_DIR_FWD;
	// Check if tape is present.
	if((in_sws&TTR_SW_TAPE_IN)==0)
	{
		// Tape is not found.
#ifdef UART_TERM
		// Check if transport was in active mode last time.
		if((*usr_mode)!=USR_MODE_STOP)
		{
			// Tape was moving, dump a message about lost tape.
			UART_add_flash_string((uint8_t *)cch_no_tape);
		}
#endif /* UART_TERM */
		// Clear user mode.
		(*usr_mode) = USR_MODE_STOP;
		if((u8_tanashin_target_mode!=TTR_TANA_MODE_HALT)&&(u8_tanashin_target_mode!=TTR_TANA_MODE_TO_INIT))
		{
			// Tape is out, clear any active mode.
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
		}
		// Turn on mute.
		MUTE_EN_ON;
		// Turn off recording circuit.
		REC_EN_OFF;
	}
	// Check if transport mode transition is in progress.
	if(u8_tanashin_trans_timer==0)
	{
		// Transport is in stable state (mode transition finished).
		// Check if transport is in error-state.
		if(u8_tanashin_mode==TTR_TANA_MODE_HALT)
		{
			// Transport control is halted due to an error, ignore any state transitions and user-requests.
			mech_tanashin_static_halt(in_sws, usr_mode);
		}
		// Check if transport has to start transitioning to another stable state.
		else if(u8_tanashin_target_mode!=u8_tanashin_mode)
		{
			// Target transport mode is not the same as current transport mode (need to start transition to another mode).
			mech_tanashin_target2mode(tacho, usr_mode);
		}
		// Transport is not in error and is in stable state (target mode reached),
		// check if user requests another mode.
		else if(mech_tanashin_user_to_transport((*usr_mode))!=u8_tanashin_target_mode)
		{
			// Not in disabled state, target mode is reached.
			// User wants another mode than current transport target is.
			mech_tanashin_user2target(usr_mode);
		}
		else
		{
			// Transport is not due to transition through modes (u8_tanashin_mode == u8_tanashin_target_mode).
			mech_tanashin_static_mode(in_features, in_sws, tacho, usr_mode);
		}
		// Check for idle timeout.
		if((in_sws&TTR_SW_TAPE_IN)==0)
		{
			// No tape is present.
			if(u16_tanashin_idle_time>=IDLE_CAP_NO_TAPE)
			{
#ifdef UART_TERM
				if(CAPSTAN_STATE!=0)
				{
					UART_add_flash_string((uint8_t *)cch_capst_stop);
				}
#endif /* UART_TERM */
				// Disable power for tacho sensor.
				TANA_TACHO_PWR_DIS;
				// Shutdown capstan motor.
				CAPSTAN_OFF;
			}
		}
		else
		{
			// Tape is loaded.
			if(u16_tanashin_idle_time>=IDLE_CAP_TAPE_IN)
			{
#ifdef UART_TERM
				if(CAPSTAN_STATE!=0)
				{
					UART_add_flash_string((uint8_t *)cch_capst_stop);
				}
#endif /* UART_TERM */
				// Disable power for tacho sensor.
				TANA_TACHO_PWR_DIS;
				// Shutdown capstan motor.
				CAPSTAN_OFF;
			}
		}
	}
	else
	{
		// Transport in transitioning through states.
		// Count down mode transition timer.
		u8_tanashin_trans_timer--;
		// Reset idle timer.
		u16_tanashin_idle_time = 0;
		// Mode transition cyclogram.
		mech_tanashin_cyclogram(in_sws);
		// Check if transition just finished.
		if(u8_tanashin_trans_timer==0)
		{
			// Reset tachometer timer.
			(*tacho) = 0;
		}
	}
	if(u8_tanashin_trans_timer==0)
	{
		DBG_MODE_ACT_OFF;
	}
	else
	{
		DBG_MODE_ACT_ON;
	}
#ifdef UART_TERM
	if((u8_tanashin_trans_timer>0)||(mech_tanashin_user_to_transport((*usr_mode))!=u8_tanashin_target_mode)||(u8_tanashin_target_mode!=u8_tanashin_mode))
	{
		sprintf(u8a_tanashin_buf, "MODE|>%03u<|%01u>%01u>%01u|%02x\n\r",
				u8_tanashin_trans_timer, (*usr_mode), u8_tanashin_target_mode, u8_tanashin_mode, in_sws);
		UART_add_string(u8a_tanashin_buf);
	}
#endif /* UART_TERM */
}

//-------------------------------------- Get user-level mode of the transport.
uint8_t mech_tanashin_get_mode()
{
    if(u8_tanashin_target_mode==TTR_TANA_MODE_PB_FWD)
    {
        return USR_MODE_PLAY_FWD;
    }
	if(u8_tanashin_target_mode==TTR_TANA_MODE_RC_FWD)
    {
        return USR_MODE_REC_FWD;
    }
    if(u8_tanashin_target_mode==TTR_TANA_MODE_FW_FWD)
    {
        return USR_MODE_FWIND_FWD;
    }
    if(u8_tanashin_target_mode==TTR_TANA_MODE_FW_REV)
    {
        return USR_MODE_FWIND_REV;
    }
    return USR_MODE_STOP;
}

//-------------------------------------- Get transition timer count.
uint8_t mech_tanashin_get_transition()
{
	return u8_tanashin_trans_timer;
}

//-------------------------------------- Get transport error.
uint8_t mech_tanashin_get_error()
{
	return u8_tanashin_error;
}

//-------------------------------------- Print transport mode alias.
void mech_tanashin_UART_dump_mode(uint8_t in_mode)
{
#ifdef UART_TERM
	if(in_mode==TTR_TANA_MODE_TO_INIT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_to_init);
	}
	else if(in_mode==TTR_TANA_SUBMODE_INIT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_init);
	}
	else if(in_mode==TTR_TANA_SUBMODE_TO_STOP)
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
	else if(in_mode==TTR_TANA_MODE_RC_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_rc_fwd);
	}
	else if(in_mode==TTR_TANA_MODE_FW_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_fwd);
	}
	else if(in_mode==TTR_TANA_MODE_FW_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_rev);
	}
	else if(in_mode==TTR_TANA_SUBMODE_TO_HALT)
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
#endif /* UART_TERM */
}
