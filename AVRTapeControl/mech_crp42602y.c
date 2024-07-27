#include "mech_crp42602y.h"

uint8_t u8_crp42602y_target_mode=TTR_42602_MODE_TO_INIT;// Target transport mode (derived from [usr_mode])
uint8_t u8_crp42602y_mode=TTR_42602_MODE_STOP;      	// Current tape transport mode (transitions to [u8_crp42602y_target_mode])
uint8_t u8_crp42602y_error=TTR_ERR_NONE;				// Last transport error
uint8_t u8_crp42602y_trans_timer=0;						// Solenoid holding timer
uint16_t u16_crp42602y_idle_time=0;						// Timer for disabling capstan motor
uint8_t u8_crp42602y_retries=0;							// Number of retries before transport halts

#ifdef UART_TERM
char u8a_crp42602y_buf[4];								// Buffer for UART debug messages
#endif /* UART_TERM */

#ifdef SUPP_CRP42602Y_MECH
volatile const uint8_t ucaf_crp42602y_mech[] PROGMEM = "CRP42602Y mechanism";
#endif /* SUPP_CRP42602Y_MECH */

//-------------------------------------- Freeze transport due to error.
void mech_crp42602y_set_error(uint8_t in_err)
{
	u8_crp42602y_target_mode = TTR_42602_MODE_HALT;
	u8_crp42602y_mode = TTR_42602_MODE_HALT;
	u8_crp42602y_error += in_err;
}

//-------------------------------------- Convert user mode to transport mode.
uint8_t mech_crp42602y_user_to_transport(uint8_t in_mode, uint8_t *play_dir)
{
	if(in_mode==USR_MODE_PLAY_FWD)
	{
		return TTR_42602_MODE_PB_FWD;
	}
	if(in_mode==USR_MODE_PLAY_REV)
	{
		return TTR_42602_MODE_PB_REV;
	}
	if(in_mode==USR_MODE_REC_FWD)
	{
		return TTR_42602_MODE_RC_FWD;
	}
	if(in_mode==USR_MODE_REC_REV)
	{
		return TTR_42602_MODE_RC_REV;
	}
	if(in_mode==USR_MODE_FWIND_FWD)
	{
		if((*play_dir)==PB_DIR_FWD)
		{
			return TTR_42602_MODE_FW_FWD;
		}
		else
		{
			return TTR_42602_MODE_FW_FWD_HD_REV;
		}
	}
	if(in_mode==USR_MODE_FWIND_REV)
	{
		if((*play_dir)==PB_DIR_FWD)
		{
			return TTR_42602_MODE_FW_REV;
		}
		else
		{
			return TTR_42602_MODE_FW_REV_HD_REV;
		}
	}
	return TTR_42602_MODE_STOP;
}

//-------------------------------------- Transport operations are halted, keep mechanism in this state.
void mech_crp42602y_static_halt(uint8_t in_sws, uint8_t *usr_mode)
{
	// Set upper levels to the same mode.
	u8_crp42602y_target_mode = TTR_42602_MODE_HALT;
	// Clear user mode.
	(*usr_mode) = USR_MODE_STOP;
	// Turn on mute.
	MUTE_EN_ON;
	// Turn off recording circuit.
	REC_EN_OFF;
	// Keep timer reset, no mode transitions.
	u8_crp42602y_trans_timer = 0;
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
		u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
		u8_crp42602y_mode = TTR_42602_SUBMODE_TO_HALT;
	}
	else
	{
		// Keep solenoid inactive.
		SOLENOID_OFF;
		// Shut down capstan motor.
		CAPSTAN_OFF;
	}
}

//-------------------------------------- Start transition from current mode to target mode.
void mech_crp42602y_target2mode(uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode)
{
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_target2current1); mech_crp42602y_UART_dump_mode(u8_crp42602y_mode);
	UART_add_flash_string((uint8_t *)cch_target2current2); mech_crp42602y_UART_dump_mode(u8_crp42602y_target_mode); UART_add_flash_string((uint8_t *)cch_endl);
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
		u8_crp42602y_target_mode = TTR_42602_MODE_TO_INIT;
	}
	// Reset idle timer.
	u16_crp42602y_idle_time = 0;
	// Reset tachometer timeout.
	(*tacho) = 0;
	if(u8_crp42602y_target_mode==TTR_42602_MODE_TO_INIT)
	{
		// Target mode: start-up delay.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_startup_delay);
#endif /* UART_TERM */
		// Set time for waiting for mechanism to stabilize.
		u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
		// Put transport in init waiting mode.
		u8_crp42602y_mode = TTR_42602_SUBMODE_INIT;
		// Move target to STOP mode.
		u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
	}
	else if(u8_crp42602y_target_mode==TTR_42602_MODE_STOP)
	{
		// Target mode: full stop.
		u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
		u8_crp42602y_mode = TTR_42602_SUBMODE_TO_STOP;
	}
	else if(u8_crp42602y_target_mode==TTR_42602_MODE_HALT)
	{
		// Target mode: full stop in HALT.
		u8_crp42602y_mode = TTR_42602_MODE_HALT;
	}
	else
	{
		// Check new target mode.
		if((u8_crp42602y_target_mode>=TTR_42602_MODE_PB_FWD)&&(u8_crp42602y_target_mode<=TTR_42602_MODE_FW_REV_HD_REV))
		{
			// Check if target mode is allowed.
			if(u8_crp42602y_target_mode==TTR_42602_MODE_RC_FWD)
			{
				if((in_sws&TTR_SW_NOREC_FWD)!=0)
				{
					// Record in forward direction is inhibited.
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_no_record);
#endif /* UART_TERM */
					// Convert RECORD to PLAYBACK.
					u8_crp42602y_target_mode = TTR_42602_MODE_PB_FWD;
				}
			}
			else if(u8_crp42602y_target_mode==TTR_42602_MODE_RC_REV)
			{
				if((in_sws&TTR_SW_NOREC_REV)!=0)
				{
					// Record in forward direction is inhibited.
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_no_record);
#endif /* UART_TERM */
					// Convert RECORD to PLAYBACK.
					u8_crp42602y_target_mode = TTR_42602_MODE_PB_REV;
				}
			}
			// Start transition to active mode.
			u8_crp42602y_trans_timer = TIM_42602_DLY_ACTIVE;
			u8_crp42602y_mode = TTR_42602_SUBMODE_TO_ACTIVE;
		}
		else
		{
			// Unknown mode, reset to STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_unknown_mode);
#endif /* UART_TERM */
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			(*usr_mode) = USR_MODE_STOP;
		}
		// Reset last error.
		u8_crp42602y_error = TTR_ERR_NONE;
	}
}

//-------------------------------------- Take in user desired mode and set new target mode.
void mech_crp42602y_user2target(uint8_t *usr_mode, uint8_t *play_dir)
{
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_user2target1); mech_crp42602y_UART_dump_mode(u8_crp42602y_target_mode);
	UART_add_flash_string((uint8_t *)cch_user2target2); UART_dump_user_mode((*usr_mode)); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
	if(u8_crp42602y_target_mode!=TTR_42602_MODE_STOP)
	{
		// Mechanism is in active mode, user wants another mode, set target as STOP, it's the only way to transition to another mode.
		// New target mode will apply in the next run of the [mech_crp42602y_state_machine()]
		// because user mode will stay the same and not equal STOP.
		u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
	}
	else
	{
		// Mechanism is in STOP mode, simple: set target to what user wants.
		u8_crp42602y_target_mode = mech_crp42602y_user_to_transport((*usr_mode), play_dir);
	}
}

//-------------------------------------- Control mechanism in static mode (not transitioning between modes).
void mech_crp42602y_static_mode(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	if(u8_crp42602y_mode==TTR_42602_MODE_STOP)
	{
		// Transport supposed to be in STOP.
		// Increase idle timer.
		if(u16_crp42602y_idle_time<IDLE_CAP_MAX)
		{
			u16_crp42602y_idle_time++;
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
			// Increase number of retries before failing.
			u8_crp42602y_retries++;
			if(u8_crp42602y_retries>=MODE_REP_MAX)
			{
				// Maximum retries, to go HALT.
				u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
				u8_crp42602y_mode = TTR_42602_SUBMODE_INIT;
				u8_crp42602y_target_mode = TTR_42602_MODE_HALT;
			}
			else
			{
				// Force STOP if transport is not in STOP.
				u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
				u8_crp42602y_mode = TTR_42602_SUBMODE_TO_STOP;
			}
		}
		else 
		{
			// Reset retry count.
			u8_crp42602y_retries = 0;
			if((in_features&TTR_FEA_STOP_TACHO)!=0)
			{
				// Check if capstan motor is running.
				if(CAPSTAN_STATE!=0)
				{
					// Checking tacho in STOP enabled.
					if((*tacho)>TACHO_42602_STOP_DLY_MAX)
					{
						// No signal from takeup tachometer for too long.
#ifdef UART_TERM
						UART_add_flash_string((uint8_t *)cch_stop_tacho); UART_add_flash_string((uint8_t *)cch_endl);
						UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop1);
#endif /* UART_TERM */
						// No motor drive or bad belts, register an error.
						mech_crp42602y_set_error(TTR_ERR_BAD_DRIVE);
					}
				}
				else
				{
					// While capstan is stopped, reset tachometer timeout.
					(*tacho) = 0;
				}
			}
		}
	}
	else if((u8_crp42602y_mode==TTR_42602_MODE_PB_FWD)||
			(u8_crp42602y_mode==TTR_42602_MODE_PB_REV)||
			(u8_crp42602y_mode==TTR_42602_MODE_RC_FWD)||
			(u8_crp42602y_mode==TTR_42602_MODE_RC_REV))
	{
		// Transport supposed to be in PLAYBACK or RECORD.
		// Reset idle timer.
		u16_crp42602y_idle_time = 0;
		// Check tachometer timer.
		if((*tacho)>TACHO_42602_PLAY_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
			// Turn mute on.
			MUTE_EN_ON;
			// Turn recording circuit off.
			REC_EN_OFF;
			// Perform auto-stop.
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			// Set default "last playback" as forward, it will be corrected below if required.
			(*play_dir) = PB_DIR_FWD;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
			// Check if reverse functions are enabled.
			if((in_features&TTR_FEA_REV_ENABLE)!=0)
			{
				// Reverse functions are allowed.
				if((in_features&TTR_FEA_PB_AUTOREV)!=0)
				{
					// Auto-reverse is allowed.
					if(u8_crp42602y_mode==TTR_42602_MODE_PB_FWD)
					{
						// Reverse ops: enabled, auto-reverse enabled.
						// Currently: playback in forward.
						// Next: playback in reverse.
#ifdef UART_TERM
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_fwd_rev);
#endif /* UART_TERM */
						// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
						(*usr_mode) = USR_MODE_PLAY_REV;
					}
					else if(u8_crp42602y_mode==TTR_42602_MODE_PB_REV)
					{
						if((in_features&TTR_FEA_PB_LOOP)!=0)
						{
							// Reverse ops: enabled, auto-reverse enabled.
							// Currently: playback in reverse, infinite loop auto-reverse is enabled.
							// Next: playback in forward.
#ifdef UART_TERM
							UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_rev_fwd);
#endif /* UART_TERM */
							// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
							(*usr_mode) = USR_MODE_PLAY_FWD;
						}
					}
					else if(u8_crp42602y_mode==TTR_42602_MODE_RC_FWD)
					{
						if((in_sws&TTR_SW_NOREC_REV)==0)
						{
							// Reverse ops: enabled, auto-reverse enabled.
							// Currently: recording in forward, recording in reverse is allowed.
							// Next: recording in reverse.
#ifdef UART_TERM
							UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_fwd_rev);
#endif /* UART_TERM */
							// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
							(*usr_mode) = USR_MODE_REC_REV;
						}
						else if((in_features&TTR_FEA_PBF2REW)!=0)
						{
							// Reverse ops: enabled, auto-reverse enabled.
							// Currently: recording in forward, recording in reverse is inhibited, auto-rewind is enabled.
							// Next: rewind.
#ifdef UART_TERM
							UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_rewind);
#endif /* UART_TERM */
							// Queue rewind.
							(*usr_mode) = USR_MODE_FWIND_REV;
						}
						else
						{
							// Reverse ops: enabled, auto-reverse enabled.
							// Currently: recording in forward, recording in reverse is inhibited, auto-rewind is disabled.
							// Next: stop.
#ifdef UART_TERM
							UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
							// Make next playback direction in reverse direction after auto-stop.
							(*play_dir) = PB_DIR_REV;
							// STOP mode already queued.
						}
					}
					else if((u8_crp42602y_mode==TTR_42602_MODE_RC_REV)&&((in_features&TTR_FEA_PB_LOOP)!=0)&&((in_sws&TTR_SW_NOREC_FWD)==0))
					{
						// Reverse ops: enabled, auto-reverse enabled.
						// Currently: recording in reverse, infinite loop auto-reverse is enabled, recording in forward is allowed.
						// Next: recording in forward.
#ifdef UART_TERM
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_rev_fwd);
#endif /* UART_TERM */
						// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
						(*usr_mode) = USR_MODE_REC_FWD;
					}
#ifdef UART_TERM
					else
					{
						// Reverse ops: enabled, auto-reverse enabled.
						// All other combinations, next mode: stop.
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
						// Stop mode already queued.
					}
#endif /* UART_TERM */
				}
				else
				{
					// Reverse functions are allowed,
					// but auto-reverse is disabled.
					if((u8_crp42602y_mode==TTR_42602_MODE_PB_FWD)||(u8_crp42602y_mode==TTR_42602_MODE_RC_FWD))
					{
						// Playback or record was in forward direction.
						if((in_features&TTR_FEA_PBF2REW)!=0)
						{
							// Reverse ops: enabled, auto-reverse disabled.
							// Currently: playback or recording in forward, auto-rewind is enabled.
							// Next: rewind.
#ifdef UART_TERM
							UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_rewind);
#endif /* UART_TERM */
							// Queue rewind.
							(*usr_mode) = USR_MODE_FWIND_REV;
						}
						else
						{
							// Reverse ops: enabled, auto-reverse disabled.
							// Currently: playback or recording in forward, auto-rewind is disabled.
							// Next: stop.
#ifdef UART_TERM
							UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
							// Make next playback direction in reverse direction after auto-stop.
							(*play_dir) = PB_DIR_REV;
							// STOP mode already queued.
						}
					}
#ifdef UART_TERM
					else
					{
						// Reverse ops: enabled, auto-reverse disabled.
						// Currently: playback or recording in reverse.
						// Next: stop.
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
						// STOP mode already queued.
					}
#endif /* UART_TERM */
				}
			}
			else if((in_features&TTR_FEA_PBF2REW)!=0)
			{
				// Reverse ops: disabled.
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
				// Reverse ops: disabled.
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
			if((u8_crp42602y_mode==TTR_42602_MODE_RC_FWD)||(u8_crp42602y_mode==TTR_42602_MODE_RC_REV))
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
			// Correct logic state to correspond with reality.
			u8_crp42602y_mode = TTR_42602_MODE_STOP;
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
	else if((u8_crp42602y_mode==TTR_42602_MODE_FW_FWD)||
			(u8_crp42602y_mode==TTR_42602_MODE_FW_REV)||
			(u8_crp42602y_mode==TTR_42602_MODE_FW_FWD_HD_REV)||
			(u8_crp42602y_mode==TTR_42602_MODE_FW_REV_HD_REV))
	{
		// Transport supposed to be in FAST WIND.
		// Keep mute on.
		MUTE_EN_ON;
		// Keep recording circuit off.
		REC_EN_OFF;
		// Reset idle timer.
		u16_crp42602y_idle_time = 0;
		// Check tachometer timer.
		if((*tacho)>TACHO_42602_FWIND_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
			// Perform auto-stop.
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			// Set default "last playback" as forward, it will be corrected below if required.
			(*play_dir) = PB_DIR_FWD;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
			
			if((u8_crp42602y_mode==TTR_42602_MODE_FW_FWD)||(u8_crp42602y_mode==TTR_42602_MODE_FW_FWD_HD_REV))
			{
				// Fast wind was in forward direction.
				if((in_features&TTR_FEA_FF2REW)!=0)
				{
					// Currently: fast wind in forward direction, auto-rewind is enabled.
					// Next: rewind.
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_rewind);
#endif /* UART_TERM */
					// Set mode to rewind.
					(*usr_mode) = USR_MODE_FWIND_REV;
				}
				else
				{
					// Currently: fast wind in forward direction, auto-rewind is disabled.
					// Next: stop.
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
					// Check if reverse functions are enabled.
					if((in_features&TTR_FEA_REV_ENABLE)!=0)
					{
						// Make next playback direction in reverse direction after auto-stop.
						(*play_dir) = PB_DIR_REV;
					}
					// STOP mode already queued.
				}
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
		}
		// Check if somehow (manually?) transport switched into STOP.
		if((in_sws&TTR_SW_STOP)!=0)
		{
			// Mechanism unexpectedly slipped into STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_stop_corr);
#endif /* UART_TERM */
			// Correct logic mode.
			u8_crp42602y_mode = TTR_42602_MODE_STOP;
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
}

//-------------------------------------- Transition through modes, timing solenoid.
void mech_crp42602y_cyclogram(uint8_t in_sws, uint8_t *play_dir)
{
	if(u8_crp42602y_mode==TTR_42602_SUBMODE_INIT)
	{
		// Desired mode: spin-up capstan, wait for TTR to stabilize.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Turn off solenoid, let mechanism stabilize.
		SOLENOID_OFF;
		// Turn on mute.
		MUTE_EN_ON;
		// Turn off recording circuit.
		REC_EN_OFF;
		if(u8_crp42602y_trans_timer==0)
		{
			// Transition is done.
			// Save new transport state.
			u8_crp42602y_mode = u8_crp42602y_target_mode;
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_TO_STOP)
	{
		// Starting transition to STOP mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Activate solenoid in to initiate mode change to STOP.
		SOLENOID_ON;
		if(u8_crp42602y_trans_timer<=(TIM_42602_DLY_WAIT_STOP-TIM_42602_DLY_STOP))
		{
			// Initial time for activating mode transition to STOP has passed.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Wait for transport to transition to STOP.
			u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_STOP;
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_WAIT_STOP)
	{
		// Transitioning to STOP mode.
		// Deactivate solenoid.
		SOLENOID_OFF;
		if(u8_crp42602y_trans_timer==0)
		{
			// Transition is done.
			// Save new transport state.
			u8_crp42602y_mode = TTR_42602_MODE_STOP;
			// Check if mechanical STOP state wasn't reached.
			if((in_sws&TTR_SW_STOP)==0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_stop_active); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_mode_failed);
				sprintf(u8a_crp42602y_buf, " %01u\n\r", u8_crp42602y_retries);
				UART_add_string(u8a_crp42602y_buf);
#endif /* UART_TERM */
				// Increase number of retries before failing.
				u8_crp42602y_retries++;
				if(u8_crp42602y_retries>=MODE_REP_MAX)
				{
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
					// Mechanically mode didn't change from active, register an error.
					mech_crp42602y_set_error(TTR_ERR_NO_CTRL);
				}
				else
				{
					// Repeat transition to STOP.
					u8_crp42602y_trans_timer = TIM_42602_DLY_WAIT_STOP;
					u8_crp42602y_mode = TTR_42602_SUBMODE_TO_STOP;
				}
			}
			else
			{
				// Reset retry count.
				u8_crp42602y_retries = 0;
			}
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_TO_ACTIVE)
	{
		// Starting transition to any active mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Activate solenoid in to initiate mode change to active mode.
		SOLENOID_ON;
		// Update last playback direction.
		if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)||(u8_crp42602y_target_mode==TTR_42602_MODE_RC_FWD))
		{
			// Direction: forward.
			(*play_dir) = PB_DIR_FWD;
		}
		else if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV)||(u8_crp42602y_target_mode==TTR_42602_MODE_RC_REV))
		{
			// Direction: reverse.
			(*play_dir) = PB_DIR_REV;
		}
		u8_crp42602y_mode = TTR_42602_SUBMODE_ACT;
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_ACT)
	{
		// Transitioning to active mode.
		// Waiting for first "gray zone" before pinch/head direction selection.
		if(u8_crp42602y_trans_timer<(TIM_42602_DLY_ACTIVE-TIM_42602_DLY_WAIT_HEAD))
		{
			// Go to the next stage.
			u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_DIR;
			// Lookup next stage solenoid state.
			if((SOLENOID_STATE!=0)&&
				((u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_RC_REV)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD_HD_REV)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_REV_HD_REV)))
			{
				// Head should be turned into REVERSE.
				// Solenoid is already on from the last stage, next stage will also have it on.
				// No need to jerk the solenoid, keep it on.
				SOLENOID_ON;
			}
			else
			{
				// Head should be turned into FORWARD.
				// Deactivate solenoid entering "gray zone".
				SOLENOID_OFF;
			}
			if((u8_crp42602y_target_mode==TTR_42602_MODE_RC_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_RC_REV))
			{
				// Turn on recording circuit (before heads contact the tape).
				REC_EN_ON;
			}
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_WAIT_DIR)
	{
		// Transitioning to active mode.
		// Waiting for first decision point: pinch/head direction.
		if(u8_crp42602y_trans_timer<(TIM_42602_DLY_ACTIVE-TIM_42602_DLY_HEAD_DIR))
		{
			// Decision point reached.
			// Pinch/head direction range.
			u8_crp42602y_mode = TTR_42602_SUBMODE_HD_DIR_SEL;
			if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_RC_REV)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD_HD_REV)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_REV_HD_REV))
			{
				// Activate solenoid in for head in reverse direction.
				SOLENOID_ON;
			}
			else
			{
				// Keep solenoid off for head in forward direction.
				SOLENOID_OFF;
			}
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_HD_DIR_SEL)
	{
		// Transitioning to active mode.
		// Waiting for second "gray zone" before pinch engage selection.
		if(u8_crp42602y_trans_timer<(TIM_42602_DLY_ACTIVE-TIM_42602_DLY_WAIT_PINCH))
		{
			// Pinch direction selection finished, wait for pinch range.
			u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_PINCH;
			// Keep solenoid off for the "gray zone".
			SOLENOID_OFF;
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_WAIT_PINCH)
	{
		// Transitioning to active mode.
		// Waiting for second decision point: pinch engage.
		if(u8_crp42602y_trans_timer<(TIM_42602_DLY_ACTIVE-TIM_42602_DLY_PINCH_EN))
		{
			// Decision point reached.
			// Pinch engage range.
			u8_crp42602y_mode = TTR_42602_SUBMODE_PINCH_SEL;
			if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_RC_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_RC_REV))
			{
				// Activate solenoid in to engage pinch roller.
				SOLENOID_ON;
			}
			else
			{
				// Keep solenoid off to select fast winding.
				SOLENOID_OFF;
			}
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_PINCH_SEL)
	{
		// Transitioning to active mode.
		// Waiting for third "gray zone" before takeup direction selection.
		if(u8_crp42602y_trans_timer<(TIM_42602_DLY_ACTIVE-TIM_42602_DLY_WAIT_TAKEUP))
		{
			// Pinch roller activation range done, wait for takeup direction range.
			u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_TAKEUP;
			// Lookup next stage solenoid state.
			if((SOLENOID_STATE!=0)&&
				((u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_RC_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD_HD_REV)))
			{
				// Solenoid is already on from the last stage, next stage will also have it on.
				// No need to jerk the solenoid, keep it on.
				SOLENOID_ON;
			}
			else
			{
				// Deactivate solenoid entering "gray zone".
				SOLENOID_OFF;
			}
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_WAIT_TAKEUP)
	{
		// Transitioning to active mode.
		// Waiting for third decision point: takeup direction.
		if(u8_crp42602y_trans_timer<(TIM_42602_DLY_ACTIVE-TIM_42602_DLY_TAKEUP_DIR))
		{
			// Decision point reached.
			// Takeup direction range.
			u8_crp42602y_mode = TTR_42602_SUBMODE_TU_DIR_SEL;
			if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_RC_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD)||
				(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD_HD_REV))
			{
				// Activate solenoid in to select takeup in forward direction.
				SOLENOID_ON;
			}
			else
			{
				// Keep solenoid off for takeup in reverse direction.
				SOLENOID_OFF;
			}
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_TU_DIR_SEL)
	{
		// Transitioning to active mode.
		// Waiting for deactivating point.
		if(u8_crp42602y_trans_timer<(TIM_42602_DLY_ACTIVE-TIM_42602_DLY_WAIT_MODE))
		{
			// Every decision was make, now just waiting for cycle to finish.
			u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_RUN;
			// Release solenoid.
			SOLENOID_OFF;
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_WAIT_RUN)
	{
		// Transitioning to active mode.
		// Waiting for cycle to finish.
		if(u8_crp42602y_trans_timer==0)
		{
			// Transition is done.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Save new transport state.
			u8_crp42602y_mode = u8_crp42602y_target_mode;
			// Check if mechanical STOP state wasn't cleared.
			if((in_sws&TTR_SW_STOP)!=0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_active_stop); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
				// Mechanically mode didn't change from STOP, register an error.
				mech_crp42602y_set_error(TTR_ERR_NO_CTRL);
				// Set default direction.
				(*play_dir) = PB_DIR_FWD;
			}
			else
			{
				// STOP condition successfully cleared.
				// Turn off mute for PLAY/RECORD modes.
				if((u8_crp42602y_mode==TTR_42602_MODE_PB_FWD)||
					(u8_crp42602y_mode==TTR_42602_MODE_RC_FWD)||
					(u8_crp42602y_mode==TTR_42602_MODE_PB_REV)||
					(u8_crp42602y_mode==TTR_42602_MODE_RC_REV))
				{
					// Turn off mute.
					MUTE_EN_OFF;
				}
				// Reset retry count.
				u8_crp42602y_retries = 0;
			}
		}
	}
	else if(u8_crp42602y_mode==TTR_42602_SUBMODE_TO_HALT)
	{
		// Starting transition to HALT mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Activate solenoid in to stop mechanism in HALT mode.
		SOLENOID_ON;
		u8_crp42602y_mode = TTR_42602_MODE_HALT;
	}
	else if(u8_crp42602y_mode==TTR_42602_MODE_HALT)
	{
		// Desired mode: recovery STOP in HALT mode.
		if(u8_crp42602y_trans_timer<=(TIM_42602_DLY_WAIT_STOP-TIM_42602_DLY_STOP))
		{
			// Initial time for activating mode transition to STOP has passed.
			// Deactivate solenoid while waiting for transport to transition to STOP.
			SOLENOID_OFF;
		}
	}
	
#ifdef UART_TERM
	if((u8_crp42602y_trans_timer==0)&&(u8_crp42602y_mode!=TTR_42602_MODE_HALT))
	{
		UART_add_flash_string((uint8_t *)cch_mode_done); UART_add_flash_string((uint8_t *)cch_arrow);
		mech_crp42602y_UART_dump_mode(u8_crp42602y_target_mode); UART_add_flash_string((uint8_t *)cch_endl);
	}
#endif /* UART_TERM */
}

//-------------------------------------- Perform tape transport state machine.
void mech_crp42602y_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	// Mode overflow protection.
	if((u8_crp42602y_mode>=TTR_42602_MODE_MAX)||(u8_crp42602y_target_mode>=TTR_42602_MODE_MAX))
	{
		// Register logic error.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_halt_stop3); UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
		mech_crp42602y_set_error(TTR_ERR_LOGIC_FAULT);
	}
	// Check if tape is present.
	if((in_sws&TTR_SW_TAPE_IN)==0)
	{
		// Tape is not found.
		// Reset last played direction if tape is removed.
		(*play_dir) = PB_DIR_FWD;
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
		if((u8_crp42602y_target_mode!=TTR_42602_MODE_HALT)&&(u8_crp42602y_target_mode!=TTR_42602_MODE_TO_INIT))
		{
			// Tape is out, clear any active mode.
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
		}
		// Turn on mute.
		MUTE_EN_ON;
		// Turn off recording circuit.
		REC_EN_OFF;
	}
	// Check if transport mode transition is in progress.
	if(u8_crp42602y_trans_timer==0)
	{
		// Transport is in stable state (mode transition finished).
		// Check if transport operations are halted.
		if(u8_crp42602y_mode==TTR_42602_MODE_HALT)
		{
			// Transport control is halted due to an error, ignore any state transitions and user-requests.
			mech_crp42602y_static_halt(in_sws, usr_mode);
		}
		// Check if current transport mode is the same as target transport mode.
		else if(u8_crp42602y_target_mode!=u8_crp42602y_mode)
		{
			// Target transport mode is not the same as current transport mode (need to start transition to another mode).
			mech_crp42602y_target2mode(in_sws, tacho, usr_mode);
		}
		// Transport is not in error and is in stable state (target mode reached),
		// check if user requests another mode.
		else if(mech_crp42602y_user_to_transport((*usr_mode), play_dir)!=u8_crp42602y_target_mode)
		{
			// Not in disabled state, target mode is reached.
			// User wants another mode than current transport target is.
			mech_crp42602y_user2target(usr_mode, play_dir);
		}
		else
		{
			// Transport is not due to transition through modes (u8_crp42602y_mode == u8_crp42602y_target_mode).
			mech_crp42602y_static_mode(in_features, in_sws, tacho, usr_mode, play_dir);
		}
		// Check for idle timeout.
		if((in_sws&TTR_SW_TAPE_IN)==0)
		{
			// No tape is present.
			if(u16_crp42602y_idle_time>=IDLE_CAP_NO_TAPE)
			{
#ifdef UART_TERM
				if(CAPSTAN_STATE!=0)
				{
					UART_add_flash_string((uint8_t *)cch_capst_stop);
				}
#endif /* UART_TERM */
				// Shutdown capstan motor.
				CAPSTAN_OFF;
			}
		}
		else
		{
			// Tape is loaded.
			if(u16_crp42602y_idle_time>=IDLE_CAP_TAPE_IN)
			{
#ifdef UART_TERM
				if(CAPSTAN_STATE!=0)
				{
					UART_add_flash_string((uint8_t *)cch_capst_stop);
				}
#endif /* UART_TERM */
				// Shutdown capstan motor.
				CAPSTAN_OFF;
			}
		}
	}
	else
	{
		// Transport is performing mode transition.
		// Count down mode transition timer.
		u8_crp42602y_trans_timer--;
		// Reset idle timer.
		u16_crp42602y_idle_time = 0;
		// Mode transition cyclogram.
		mech_crp42602y_cyclogram(in_sws, play_dir);
		// Check if transition just finished.
		if(u8_crp42602y_trans_timer==0)
		{
			// Reset tachometer timer.
			(*tacho) = 0;
		}
	}
	if(u8_crp42602y_trans_timer==0)
	{
		DBG_MODE_ACT_OFF;
	}
	else
	{
		DBG_MODE_ACT_ON;
	}
}

//-------------------------------------- Get user-level mode of the transport.
uint8_t mech_crp42602y_get_mode()
{
    if(u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)
    {
        return USR_MODE_PLAY_FWD;
    }
    if(u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV)
    {
        return USR_MODE_PLAY_REV;
    }
	if(u8_crp42602y_target_mode==TTR_42602_MODE_RC_FWD)
    {
        return USR_MODE_REC_FWD;
    }
	if(u8_crp42602y_target_mode==TTR_42602_MODE_RC_REV)
    {
        return USR_MODE_REC_REV;
    }
    if((u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD)||(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD_HD_REV))
    {
        return USR_MODE_FWIND_FWD;
    }
    if((u8_crp42602y_target_mode==TTR_42602_MODE_FW_REV)||(u8_crp42602y_target_mode==TTR_42602_MODE_FW_REV_HD_REV))
    {
        return USR_MODE_FWIND_REV;
    }
    return USR_MODE_STOP;
}

//-------------------------------------- Get transition timer count.
uint8_t mech_crp42602y_get_transition()
{
	return u8_crp42602y_trans_timer;
}

//-------------------------------------- Get transport error.
uint8_t mech_crp42602y_get_error()
{
	return u8_crp42602y_error;
}

//-------------------------------------- Print transport mode alias.
void mech_crp42602y_UART_dump_mode(uint8_t in_mode)
{
#ifdef UART_TERM
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
	else if(in_mode==TTR_42602_MODE_RC_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_rc_fwd);
	}
	else if(in_mode==TTR_42602_MODE_RC_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_rc_rev);
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
		if(in_mode==TTR_42602_MODE_TO_INIT)
		{
			UART_add_flash_string((uint8_t *)cch_mode_to_init);
		}
		else if(in_mode==TTR_42602_SUBMODE_INIT)
		{
			UART_add_flash_string((uint8_t *)cch_mode_init);
		}
		else if(in_mode==TTR_42602_SUBMODE_TO_STOP)
		{
			UART_add_flash_string((uint8_t *)cch_mode_to_stop);
		}
		else if(in_mode==TTR_42602_SUBMODE_WAIT_STOP)
		{
			UART_add_flash_string((uint8_t *)cch_mode_wait_stop);
		}
		else if(in_mode==TTR_42602_SUBMODE_TO_ACTIVE)
		{
			UART_add_flash_string((uint8_t *)cch_mode_to_start);
		}
		else if(in_mode==TTR_42602_SUBMODE_WAIT_DIR)
		{
			UART_add_flash_string((uint8_t *)cch_mode_wait_dir);
		}
		else if(in_mode==TTR_42602_SUBMODE_HD_DIR_SEL)
		{
			UART_add_flash_string((uint8_t *)cch_mode_hd_dir_sel);
		}
		else if(in_mode==TTR_42602_SUBMODE_WAIT_PINCH)
		{
			UART_add_flash_string((uint8_t *)cch_mode_wait_pinch);
		}
		else if(in_mode==TTR_42602_SUBMODE_PINCH_SEL)
		{
			UART_add_flash_string((uint8_t *)cch_mode_pinch_sel);
		}
		else if(in_mode==TTR_42602_SUBMODE_WAIT_TAKEUP)
		{
			UART_add_flash_string((uint8_t *)cch_mode_wait_takeup);
		}
		else if(in_mode==TTR_42602_SUBMODE_TU_DIR_SEL)
		{
			UART_add_flash_string((uint8_t *)cch_mode_tu_dir_sel);
		}
		else if(in_mode==TTR_42602_SUBMODE_WAIT_RUN)
		{
			UART_add_flash_string((uint8_t *)cch_mode_wait_run);
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
#endif /* UART_TERM */
}
