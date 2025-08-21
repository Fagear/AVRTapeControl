#include "mech_knwd.h"

uint8_t u8_knwd_target_mode=TTR_KNWD_MODE_TO_INIT;	// Target transport mode (derived from [usr_mode])
uint8_t u8_knwd_mode=TTR_KNWD_MODE_STOP;      		// Current tape transport mode (transitions to [u8_knwd_target_mode])
uint8_t u8_knwd_error=TTR_ERR_NONE;					// Last transport error
uint8_t u8_knwd_trans_timer=0;						// Solenoid holding timer
uint16_t u16_knwd_idle_time=0;						// Timer for disabling capstan motor
uint8_t u8_knwd_retries=0;							// Number of retries before transport halts

#ifdef UART_TERM
char u8a_knwd_buf[32];								// Buffer for UART debug messages
#endif /* UART_TERM */

#ifdef SUPP_KENWOOD_MECH
volatile const uint8_t ucaf_knwd_mech[] PROGMEM = "Kenwood mechanism";
#endif /* SUPP_KENWOOD_MECH */

//-------------------------------------- Freeze transport due to error.
void mech_knwd_set_error(uint8_t in_err)
{
	u8_knwd_target_mode = TTR_KNWD_MODE_HALT;
	u8_knwd_mode = TTR_KNWD_MODE_HALT;
	u8_knwd_error += in_err;
}

//-------------------------------------- Convert user mode to transport mode.
uint8_t mech_knwd_user_to_transport(uint8_t in_mode, uint8_t *play_dir)
{
	if(in_mode==USR_MODE_PLAY_FWD)
	{
		return TTR_KNWD_MODE_PB_FWD;
	}
	if(in_mode==USR_MODE_PLAY_REV)
	{
		return TTR_KNWD_MODE_PB_REV;
	}
	if(in_mode==USR_MODE_REC_FWD)
	{
		return TTR_KNWD_MODE_RC_FWD;
	}
	if(in_mode==USR_MODE_REC_REV)
	{
		return TTR_KNWD_MODE_RC_REV;
	}
	if(in_mode==USR_MODE_FWIND_FWD)
	{
		if((*play_dir)==PB_DIR_FWD)
		{
			return TTR_KNWD_MODE_FW_FWD;
		}
		else
		{
			return TTR_KNWD_MODE_FW_FWD_HD_REV;
		}
	}
	if(in_mode==USR_MODE_FWIND_REV)
	{
		if((*play_dir)==PB_DIR_FWD)
		{
			return TTR_KNWD_MODE_FW_REV;
		}
		else
		{
			return TTR_KNWD_MODE_FW_REV_HD_REV;
		}
	}
	return TTR_KNWD_MODE_STOP;
}

//-------------------------------------- Transport operations are halted, keep mechanism in this state.
void mech_knwd_static_halt(uint8_t in_sws, uint8_t *usr_mode)
{
	// Set upper levels to the same mode.
	u8_knwd_target_mode = TTR_KNWD_MODE_HALT;
	// Clear user mode.
	(*usr_mode) = USR_MODE_STOP;
	// Turn on mute.
	MUTE_EN_ON;
	// Turn off recording circuit.
	REC_EN_OFF;
	// Keep timer reset, no mode transitions.
	u8_knwd_trans_timer = 0;
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
		u8_knwd_trans_timer = TIM_KNWD_DLY_WAIT_STOP;
		u8_knwd_mode = TTR_KNWD_SUBMODE_TO_HALT;
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
void mech_knwd_target2mode(uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode)
{
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_target2current1); mech_knwd_UART_dump_mode(u8_knwd_mode);
	UART_add_flash_string((uint8_t *)cch_target2current2); mech_knwd_UART_dump_mode(u8_knwd_target_mode); UART_add_flash_string((uint8_t *)cch_endl);
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
		u8_knwd_target_mode = TTR_KNWD_MODE_TO_INIT;
	}
	// Reset idle timer.
	u16_knwd_idle_time = 0;
	// Reset tachometer timeout.
	(*tacho) = 0;
	if(u8_knwd_target_mode==TTR_KNWD_MODE_TO_INIT)
	{
		// Target mode: start-up delay.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_startup_delay);
#endif /* UART_TERM */
		// Set time for waiting for mechanism to stabilize.
		u8_knwd_trans_timer = TIM_KNWD_DLY_ACTIVE;
		// Put transport in init waiting mode.
		u8_knwd_mode = TTR_KNWD_SUBMODE_INIT;
		// Move target to STOP mode.
		u8_knwd_target_mode = TTR_KNWD_MODE_STOP;
	}
	else if(u8_knwd_target_mode==TTR_KNWD_MODE_STOP)
	{
		// Target mode: full STOP.
		if((u8_knwd_mode==TTR_KNWD_MODE_PB_FWD)||
			(u8_knwd_mode==TTR_KNWD_MODE_PB_REV)||
			(u8_knwd_mode==TTR_KNWD_MODE_RC_FWD)||
			(u8_knwd_mode==TTR_KNWD_MODE_RC_REV))
		{
			// From playback/record there only way is to fast wind, need to get through that.
			u8_knwd_trans_timer = TIM_KNWD_DLY_FWIND_WAIT;
			u8_knwd_mode = TTR_KNWD_SUBMODE_TO_FWIND;
		}
		else if((u8_knwd_mode==TTR_KNWD_MODE_FW_FWD)||
				(u8_knwd_mode==TTR_KNWD_MODE_FW_REV)||
				(u8_knwd_mode==TTR_KNWD_MODE_FW_FWD_HD_REV)||
				(u8_knwd_mode==TTR_KNWD_MODE_FW_REV_HD_REV))
		{
			// Next from fast wind is STOP.
			u8_knwd_trans_timer = TIM_KNWD_DLY_STOP;
			u8_knwd_mode = TTR_KNWD_SUBMODE_TO_STOP;
		}
		else
		{
			// TTR is in unknown state.
			mech_knwd_set_error(TTR_ERR_LOGIC_FAULT);
		}
	}
	else if(u8_knwd_target_mode==TTR_KNWD_MODE_HALT)
	{
		// Target mode: full stop in HALT.
		u8_knwd_mode = TTR_KNWD_MODE_HALT;
	}
	else
	{
		// TODO
		// Check new target mode.
		if((u8_knwd_target_mode==TTR_KNWD_MODE_PB_FWD)||
			(u8_knwd_target_mode==TTR_KNWD_MODE_RC_FWD)||
			(u8_knwd_target_mode==TTR_KNWD_MODE_PB_REV)||
			(u8_knwd_target_mode==TTR_KNWD_MODE_RC_REV))
		{
			// Target mode: PLAYBACK/RECORD.
			if(u8_knwd_mode==TTR_KNWD_MODE_STOP)
			{
				// Playback/record can be selected only from STOP.
				u8_knwd_trans_timer = TIM_KNWD_DLY_PB_WAIT;
				u8_knwd_mode = TTR_KNWD_SUBMODE_TO_PLAY;
			}
			else if((u8_knwd_mode==TTR_KNWD_MODE_FW_FWD)||
					(u8_knwd_mode==TTR_KNWD_MODE_FW_REV)||
					(u8_knwd_mode==TTR_KNWD_MODE_FW_FWD_HD_REV)||
					(u8_knwd_mode==TTR_KNWD_MODE_FW_REV_HD_REV))
			{
				// From fast wind the mode has to become STOP at first.
				u8_knwd_trans_timer = TIM_KNWD_DLY_STOP;
				u8_knwd_mode = TTR_KNWD_SUBMODE_TO_STOP;
			}
			else
			{
				// TTR is in unknown state.
				mech_knwd_set_error(TTR_ERR_LOGIC_FAULT);
			}
		}
		else if((u8_knwd_target_mode==TTR_KNWD_MODE_FW_FWD)||
				(u8_knwd_target_mode==TTR_KNWD_MODE_FW_REV)||
				(u8_knwd_target_mode==TTR_KNWD_MODE_FW_FWD_HD_REV)||
				(u8_knwd_target_mode==TTR_KNWD_MODE_FW_REV_HD_REV))
		{
			// Target mode: FAST WIND.
		}
		if((u8_knwd_target_mode>=TTR_KNWD_MODE_PB_FWD)&&(u8_knwd_target_mode<=TTR_KNWD_MODE_FW_REV_HD_REV))
		{
			// Check if target mode is allowed.
			if(u8_knwd_target_mode==TTR_KNWD_MODE_RC_FWD)
			{
				if((in_sws&TTR_SW_NOREC_FWD)!=0)
				{
					// Record in forward direction is inhibited.
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_no_record);
#endif /* UART_TERM */
					// Convert RECORD to PLAYBACK.
					u8_knwd_target_mode = TTR_KNWD_MODE_PB_FWD;
				}
			}
			else if(u8_knwd_target_mode==TTR_KNWD_MODE_RC_REV)
			{
				if((in_sws&TTR_SW_NOREC_REV)!=0)
				{
					// Record in forward direction is inhibited.
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_no_record);
#endif /* UART_TERM */
					// Convert RECORD to PLAYBACK.
					u8_knwd_target_mode = TTR_KNWD_MODE_PB_REV;
				}
			}
			// Start transition to active mode.
			u8_knwd_trans_timer = TIM_KNWD_DLY_ACTIVE;
			//u8_knwd_mode = TTR_KNWD_SUBMODE_TO_ACTIVE;
		}
		else
		{
			// Unknown mode, reset to STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_unknown_mode);
#endif /* UART_TERM */
			u8_knwd_target_mode = TTR_KNWD_MODE_STOP;
			(*usr_mode) = USR_MODE_STOP;
		}
		// Reset last error.
		u8_knwd_error = TTR_ERR_NONE;
	}
}

//-------------------------------------- Take in user desired mode and set new target mode.
void mech_knwd_user2target(uint8_t *usr_mode, uint8_t *play_dir)
{
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_user2target1); mech_knwd_UART_dump_mode(u8_knwd_target_mode);
	UART_add_flash_string((uint8_t *)cch_user2target2); UART_dump_user_mode((*usr_mode)); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
	// New target mode will apply in the next run of the [mech_knwd_state_machine()].
	u8_knwd_target_mode = mech_knwd_user_to_transport((*usr_mode), play_dir);
}

//-------------------------------------- Control mechanism in static mode (not transitioning between modes).
void mech_knwd_static_mode(uint8_t in_ttr_features, uint8_t in_srv_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	if(u8_knwd_mode==TTR_KNWD_MODE_STOP)
	{
		// Transport supposed to be in STOP.
		// Increase idle timer.
		if(u16_knwd_idle_time<IDLE_CAP_MAX)
		{
			u16_knwd_idle_time++;
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
			u8_knwd_trans_timer = TIM_KNWD_DLY_WAIT_STOP;		// Load maximum delay to allow mechanism to revert to STOP (before retrying)
			u8_knwd_mode = TTR_KNWD_SUBMODE_TO_STOP;			// Set mode to trigger solenoid
			u8_knwd_target_mode = TTR_KNWD_MODE_STOP;			// Set target to be STOP
		}
	}
	else if((u8_knwd_mode==TTR_KNWD_MODE_PB_FWD)||
			(u8_knwd_mode==TTR_KNWD_MODE_PB_REV)||
			(u8_knwd_mode==TTR_KNWD_MODE_RC_FWD)||
			(u8_knwd_mode==TTR_KNWD_MODE_RC_REV))
	{
		// Transport supposed to be in PLAYBACK or RECORD.
		// Reset idle timer.
		u16_knwd_idle_time = 0;
		// Check tachometer timer.
		if((*tacho)>TACHO_KNWD_PLAY_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
			// Perform auto-stop.
			u8_knwd_target_mode = TTR_KNWD_MODE_STOP;
			// Set default "last playback" as forward, it will be corrected below if required.
			(*play_dir) = PB_DIR_FWD;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
			// Check if reverse functions are enabled.
			if((in_ttr_features&TTR_FEA_REV_ENABLE)!=0)
			{
				// Reverse functions are allowed.
				if((in_srv_features&SRV_FEA_PB_AUTOREV)!=0)
				{
					// Auto-reverse is allowed.
					if(u8_knwd_mode==TTR_KNWD_MODE_PB_FWD)
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
					else if(u8_knwd_mode==TTR_KNWD_MODE_PB_REV)
					{
						if((in_srv_features&SRV_FEA_PB_LOOP)!=0)
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
					else if(u8_knwd_mode==TTR_KNWD_MODE_RC_FWD)
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
						else if((in_srv_features&SRV_FEA_PBF2REW)!=0)
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
					else if((u8_knwd_mode==TTR_KNWD_MODE_RC_REV)&&((in_srv_features&SRV_FEA_PB_LOOP)!=0)&&((in_sws&TTR_SW_NOREC_FWD)==0))
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
					if((u8_knwd_mode==TTR_KNWD_MODE_PB_FWD)||(u8_knwd_mode==TTR_KNWD_MODE_RC_FWD))
					{
						// Playback or record was in forward direction.
						if((in_srv_features&SRV_FEA_PBF2REW)!=0)
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
			else if((in_srv_features&SRV_FEA_PBF2REW)!=0)
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
			if((u8_knwd_mode==TTR_KNWD_MODE_RC_FWD)||(u8_knwd_mode==TTR_KNWD_MODE_RC_REV))
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
			u8_knwd_mode = TTR_KNWD_MODE_STOP;
			u8_knwd_target_mode = TTR_KNWD_MODE_STOP;
			u8_knwd_trans_timer = TIM_KNWD_DLY_WAIT_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
	else if((u8_knwd_mode==TTR_KNWD_MODE_FW_FWD)||
			(u8_knwd_mode==TTR_KNWD_MODE_FW_REV)||
			(u8_knwd_mode==TTR_KNWD_MODE_FW_FWD_HD_REV)||
			(u8_knwd_mode==TTR_KNWD_MODE_FW_REV_HD_REV))
	{
		// Transport supposed to be in FAST WIND.
		// Keep mute on.
		MUTE_EN_ON;
		// Keep recording circuit off.
		REC_EN_OFF;
		// Reset idle timer.
		u16_knwd_idle_time = 0;
		// Check tachometer timer.
		if((*tacho)>TACHO_KNWD_FWIND_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
			// Perform auto-stop.
			u8_knwd_target_mode = TTR_KNWD_MODE_STOP;
			// Set default "last playback" as forward, it will be corrected below if required.
			(*play_dir) = PB_DIR_FWD;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
			
			if((u8_knwd_mode==TTR_KNWD_MODE_FW_FWD)||(u8_knwd_mode==TTR_KNWD_MODE_FW_FWD_HD_REV))
			{
				// Fast wind was in forward direction.
				if((in_srv_features&SRV_FEA_FF2REW)!=0)
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
					if((in_ttr_features&TTR_FEA_REV_ENABLE)!=0)
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
			u8_knwd_mode = TTR_KNWD_MODE_STOP;
			u8_knwd_target_mode = TTR_KNWD_MODE_STOP;
			u8_knwd_trans_timer = TIM_KNWD_DLY_WAIT_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
}

//-------------------------------------- Transition through modes, timing solenoid.
void mech_knwd_cyclogram(uint8_t in_sws, uint8_t *play_dir)
{
	if(u8_knwd_mode==TTR_KNWD_SUBMODE_INIT)
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
		if(u8_knwd_trans_timer==0)
		{
			// Transition is done.
			// Save new transport state.
			u8_knwd_mode = u8_knwd_target_mode;
		}
	}
	else if(u8_knwd_mode==TTR_KNWD_SUBMODE_TO_STOP)
	{
		// Starting transition to STOP mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Pull solenoid in to initiate mode change to STOP.
		SOLENOID_ON;
		if(u8_knwd_trans_timer<=(TIM_KNWD_DLY_WAIT_STOP-TIM_KNWD_DLY_STOP))
		{
			// Initial time for activating mode transition to STOP has passed.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Wait for transport to transition to STOP.
			u8_knwd_mode = TTR_KNWD_SUBMODE_WAIT_STOP;
		}
	}
	else if(u8_knwd_mode==TTR_KNWD_SUBMODE_WAIT_STOP)
	{
		// Transitioning to STOP mode.
		// Deactivate solenoid.
		SOLENOID_OFF;
		if(u8_knwd_trans_timer==0)
		{
			// Transition is done.
			// Set stable state.
			u8_knwd_mode = TTR_KNWD_MODE_STOP;
			// Check if mechanical STOP state wasn't reached.
			if((in_sws&TTR_SW_STOP)==0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_stop_active); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_mode_failed);
				sprintf(u8a_knwd_buf, " %01u\n\r", u8_knwd_retries);
				UART_add_string(u8a_knwd_buf);
#endif /* UART_TERM */
				// Increase number of retries before failing.
				u8_knwd_retries++;
				if(u8_knwd_retries>=MODE_REP_MAX)
				{
#ifdef UART_TERM
					UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
					// Mechanically mode didn't change from active, register an error.
					mech_knwd_set_error(TTR_ERR_NO_CTRL);
				}
				else
				{
					// Repeat transition to STOP.
					u8_knwd_trans_timer = TIM_KNWD_DLY_STOP;
					u8_knwd_mode = TTR_KNWD_SUBMODE_TO_STOP;
				}
			}
			else
			{
				// Reset retry count.
				u8_knwd_retries = 0;
			}
		}
	}
	else if(u8_knwd_mode==TTR_KNWD_SUBMODE_TO_PLAY)
	{
		// Starting transition to PLAY/RECORD mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Activate solenoid to start transition to PLAY/RECORD.
		SOLENOID_ON;
		if(u8_knwd_target_mode==TTR_KNWD_MODE_RC_FWD)
		{
			// Turn on recording circuit (before heads contact the tape).
			REC_EN_ON;
		}
		u8_knwd_mode = TTR_KNWD_SUBMODE_WAIT_PLAY;
	}
	else if(u8_knwd_mode==TTR_KNWD_SUBMODE_WAIT_PLAY)
	{
		// Transitioning to PLAY/RECORD mode.
		if(u8_knwd_trans_timer==0)
		{
			// Transition is done.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Set stable state.
			u8_knwd_mode = u8_knwd_target_mode;
			// Check if mechanical STOP state wasn't cleared.
			if((in_sws&TTR_SW_STOP)!=0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_active_stop); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
				// Mechanically mode didn't change from STOP, register an error.
				mech_knwd_set_error(TTR_ERR_NO_CTRL);
			}
			else
			{
				// Turn off mute.
				MUTE_EN_OFF;
			}
		}
		// TODO: PLAY cyclogram
		else if(u8_knwd_trans_timer<(TIM_KNWD_DLY_PB_WAIT-TIM_KNWD_DLY_SW_ACT))
		{
			// Deactivate solenoid and wait for transition to happen.
			SOLENOID_OFF;
		}
	}
	else if(u8_knwd_mode==TTR_KNWD_SUBMODE_TO_FWIND)
	{
		// Starting transition to FAST WIND mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Activate solenoid to start transition to FAST WIND.
		SOLENOID_ON;
		u8_knwd_mode = TTR_KNWD_SUBMODE_WAIT_FWIND;
	}
	else if(u8_knwd_mode==TTR_KNWD_SUBMODE_WAIT_FWIND)
	{
		// Transitioning to FAST WIND mode.
		if(u8_knwd_trans_timer==0)
		{
			// Transition is done.
			// Deactivate solenoid.
			SOLENOID_OFF;
			// Set stable state.
			u8_knwd_mode = u8_knwd_target_mode;
			// Check if mechanical STOP state suddenly appeared.
			if((in_sws&TTR_SW_STOP)!=0)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_active_stop); UART_add_flash_string((uint8_t *)cch_endl);
				UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
				// Mechanically mode slipped to STOP, register an error.
				mech_knwd_set_error(TTR_ERR_NO_CTRL);
			}
		}
		// TODO: FAST WIND cyclogram
		/*else if(u8_knwd_trans_timer<(TIM_KNWD_DLY_FWIND_WAIT-TIM_KNWD_DLY_FWIND_ACT))
		{
			// Deactivate solenoid and wait for transition to happen.
			SOLENOID_OFF;
		}
		else if(u8_knwd_trans_timer<(TIM_KNWD_DLY_FWIND_WAIT-TIM_KNWD_DLY_WAIT_REW_ACT))
		{
			// Check takeup direction for FAST WIND.
			if(u8_knwd_target_mode==TTR_KNWD_MODE_FW_REV)
			{
				// Activate solenoid for reverse direction.
				SOLENOID_ON;
			}
		}*/
		else if(u8_knwd_trans_timer<(TIM_KNWD_DLY_FWIND_WAIT-TIM_KNWD_DLY_SW_ACT))
		{
			// Deactivate solenoid and wait for takeup selection range.
			SOLENOID_OFF;
		}
	}
	else if(u8_knwd_mode==TTR_KNWD_SUBMODE_TO_HALT)
	{
		// Starting transition to HALT mode.
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Activate solenoid to start transition to HALT.
		SOLENOID_ON;
		u8_knwd_mode = TTR_KNWD_MODE_HALT;
	}
	else if(u8_knwd_mode==TTR_KNWD_MODE_HALT)
	{
		// Desired mode: recovery STOP in HALT mode.
		if(u8_knwd_trans_timer<(TIM_KNWD_DLY_ACTIVE-TIM_KNWD_DLY_SW_ACT))
		{
			// Initial time for activating mode transition to STOP has passed.
			// Release solenoid while waiting for transport to transition to STOP.
			SOLENOID_OFF;
		}
	}
	
#ifdef UART_TERM
	if((u8_knwd_trans_timer==0)&&(u8_knwd_mode!=TTR_KNWD_MODE_HALT))
	{
		UART_add_flash_string((uint8_t *)cch_mode_done); UART_add_flash_string((uint8_t *)cch_arrow);
		mech_knwd_UART_dump_mode(u8_knwd_mode); UART_add_flash_string((uint8_t *)cch_endl);
	}
#endif /* UART_TERM */
}

//-------------------------------------- Perform tape transport state machine.
void mech_knwd_state_machine(uint8_t in_ttr_features, uint8_t in_srv_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	// Mode overflow protection.
	if((u8_knwd_mode>=TTR_KNWD_MODE_MAX)||(u8_knwd_target_mode>=TTR_KNWD_MODE_MAX))
	{
		// Register logic error.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_halt_stop3); UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
		mech_knwd_set_error(TTR_ERR_LOGIC_FAULT);
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
		if((u8_knwd_target_mode!=TTR_KNWD_MODE_HALT)&&(u8_knwd_target_mode!=TTR_KNWD_MODE_TO_INIT))
		{
			// Tape is out, clear any active mode.
			u8_knwd_target_mode = TTR_KNWD_MODE_STOP;
		}
		// Turn on mute.
		MUTE_EN_ON;
		// Turn off recording circuit.
		REC_EN_OFF;
	}
	// Check if transport mode transition is in progress.
	if(u8_knwd_trans_timer==0)
	{
		// Transport is in stable state (mode transition finished).
		// Check if transport operations are halted.
		if(u8_knwd_mode==TTR_KNWD_MODE_HALT)
		{
			// Transport control is halted due to an error, ignore any state transitions and user-requests.
			mech_knwd_static_halt(in_sws, usr_mode);
		}
		// Check if current transport mode is the same as target transport mode.
		else if(u8_knwd_target_mode!=u8_knwd_mode)
		{
			// Target transport mode is not the same as current transport mode (need to start transition to another mode).
			mech_knwd_target2mode(in_sws, tacho, usr_mode);
		}
		// Transport is not in error and is in stable state (target mode reached),
		// check if user requests another mode.
		else if(mech_knwd_user_to_transport((*usr_mode), play_dir)!=u8_knwd_target_mode)
		{
			// Not in disabled state, target mode is reached.
			// User wants another mode than current transport target is.
			mech_knwd_user2target(usr_mode, play_dir);
		}
		else
		{
			// Transport is not due to transition through modes (u8_knwd_mode == u8_knwd_target_mode).
			mech_knwd_static_mode(in_ttr_features, in_srv_features, in_sws, tacho, usr_mode, play_dir);
		}
		// Check for idle timeout.
		if((in_sws&TTR_SW_TAPE_IN)==0)
		{
			// No tape is present.
			if(u16_knwd_idle_time>=IDLE_CAP_NO_TAPE)
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
			if(u16_knwd_idle_time>=IDLE_CAP_TAPE_IN)
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
		u8_knwd_trans_timer--;
		// Reset idle timer.
		u16_knwd_idle_time = 0;
		// Mode transition cyclogram.
		mech_knwd_cyclogram(in_sws, play_dir);
		// Check if transition just finished.
		if(u8_knwd_trans_timer==0)
		{
			// Reset tachometer timer.
			(*tacho) = 0;
		}
	}
	if(u8_knwd_trans_timer==0)
	{
		DBG_MODE_ACT_OFF;
	}
	else
	{
		DBG_MODE_ACT_ON;
	}
}

//-------------------------------------- Get user-level mode of the transport.
uint8_t mech_knwd_get_mode()
{
	if(u8_knwd_target_mode==TTR_KNWD_MODE_PB_FWD)
	{
		return USR_MODE_PLAY_FWD;
	}
	if(u8_knwd_target_mode==TTR_KNWD_MODE_PB_REV)
	{
		return USR_MODE_PLAY_REV;
	}
	if(u8_knwd_target_mode==TTR_KNWD_MODE_RC_FWD)
	{
		return USR_MODE_REC_FWD;
	}
	if(u8_knwd_target_mode==TTR_KNWD_MODE_RC_REV)
	{
		return USR_MODE_REC_REV;
	}
	if((u8_knwd_target_mode==TTR_KNWD_MODE_FW_FWD)||(u8_knwd_target_mode==TTR_KNWD_MODE_FW_FWD_HD_REV))
	{
		return USR_MODE_FWIND_FWD;
	}
	if((u8_knwd_target_mode==TTR_KNWD_MODE_FW_REV)||(u8_knwd_target_mode==TTR_KNWD_MODE_FW_REV_HD_REV))
	{
		return USR_MODE_FWIND_REV;
	}
	return USR_MODE_STOP;
}

//-------------------------------------- Get transition timer count.
uint8_t mech_knwd_get_transition()
{
	return u8_knwd_trans_timer;
}

//-------------------------------------- Get transport error.
uint8_t mech_knwd_get_error()
{
	return u8_knwd_error;
}

//-------------------------------------- Print transport mode alias.
void mech_knwd_UART_dump_mode(uint8_t in_mode)
{
#ifdef UART_TERM
	if(in_mode==TTR_KNWD_MODE_STOP)
	{
		UART_add_flash_string((uint8_t *)cch_mode_stop);
	}
	else if(in_mode==TTR_KNWD_MODE_PB_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_pb_fwd);
	}
	else if(in_mode==TTR_KNWD_MODE_PB_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_pb_rev);
	}
	else if(in_mode==TTR_KNWD_MODE_RC_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_rc_fwd);
	}
	else if(in_mode==TTR_KNWD_MODE_RC_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_rc_rev);
	}
	else if(in_mode==TTR_KNWD_MODE_FW_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_fwd);
	}
	else if(in_mode==TTR_KNWD_MODE_FW_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_fw_rev);
	}
	else if(in_mode==TTR_KNWD_MODE_HALT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_halt);
	}
	else
	{
		// TODO: Possible add more modes
		UART_add_flash_string((uint8_t *)cch_mode_unknown);
	}
#endif /* UART_TERM */
}
