#include "mech_tanashin.h"

uint8_t u8_tanashin_target_mode=TTR_TANA_MODE_TO_INIT;	// Target transport mode (derived from [usr_mode])
uint8_t u8_tanashin_mode=TTR_TANA_MODE_STOP;		// Current tape transport mode (transitions to [u8_tanashin_target_mode])
uint8_t u8_tanashin_error=TTR_ERR_NONE;				// Last transport error
uint8_t u8_tanashin_trans_timer=0;					// Solenoid holding timer
uint16_t u16_tanashin_idle_time=0;					// Timer for disabling capstan motor

#ifdef UART_TERM
char u8a_buf[48];									// Buffer for UART debug messages
#endif /* UART_TERM */

volatile const uint8_t ucaf_tanashin_mech[] PROGMEM = "Tanashin-clone mechanism";

//-------------------------------------- Convert user mode to transport mode for Tanashin-clone mechanism.
uint8_t mech_tanashin_user_to_transport(uint8_t in_mode)
{
	if(in_mode==USR_MODE_PLAY_FWD)
	{
		return TTR_TANA_MODE_PB_FWD;
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
		u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;				// Load maximum delay to allow mechanism to revert to STOP (before retrying)
		u8_tanashin_target_mode = TTR_TANA_MODE_TO_HALT;			// Set mode to trigger solenoid
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
void mech_tanashin_target2mode(uint8_t *tacho, uint8_t *usr_mode)
{
#ifdef UART_TERM
	UART_add_flash_string((uint8_t *)cch_target2current1); mech_tanashin_UART_dump_mode(u8_tanashin_mode);
	UART_add_flash_string((uint8_t *)cch_target2current2); mech_tanashin_UART_dump_mode(u8_tanashin_target_mode); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
	// For any mode transition capstan must be running.
	// Check if capstan is running.
	if(CAPSTAN_STATE==0)
	{
		// Capstan is stopped.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_capst_start);
#endif /* UART_TERM */
		// Turn on capstan motor.
		CAPSTAN_ON;
		// Override target mode to perform capstan spinup wait.
		u8_tanashin_target_mode = TTR_TANA_MODE_TO_INIT;
		// Reset tachometer timeout.
		(*tacho) = 0;
	}
	// Reset idle timer.
	u16_tanashin_idle_time = 0;
	if(u8_tanashin_target_mode==TTR_TANA_MODE_TO_INIT)
	{
		// Target mode: start-up delay.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_startup_delay);
#endif /* UART_TERM */
		// Set time for waiting for mechanism to stabilize.
		u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;
		// Put transport in init waiting mode.
		u8_tanashin_mode = TTR_TANA_MODE_INIT;
		// Move target to STOP mode.
		u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
		// Clear user mode.
		(*usr_mode) = USR_MODE_STOP;
		SOLENOID_OFF;
	}
	else if(u8_tanashin_target_mode==TTR_TANA_MODE_STOP)
	{
		// Target mode: full stop.
		if(u8_tanashin_mode==TTR_TANA_MODE_PB_FWD)
		{
			// From playback there is only way to fast wind, need to get through that.
		}
		else if((u8_tanashin_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_FW_REV))
		{
			// Next from fast wind is stop.
		}
		else
		{
			// TTR is in unknown state.
		}

		u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;
		u8_tanashin_mode = TTR_TANA_SUBMODE_TO_STOP;
	}
	else
	{
		// Reset tachometer timer.
		//(*tacho) = 0;
		// Check new target mode.
		// TODO: select transition scheme from current mode & target mode.
		if((u8_tanashin_target_mode==TTR_TANA_MODE_PB_FWD)
			||(u8_tanashin_target_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_target_mode==TTR_TANA_MODE_FW_REV))
		{
			// Start transition to active mode.
#ifdef UART_TERM
			UART_add_string("New: active\n\r");
#endif /* UART_TERM */
			//u8_tanashin_trans_timer = TIM_42602_DELAY_RUN;
			//u8_tanashin_mode = TTR_42602_SUBMODE_TO_START;
		}
		else
		{
			// Unknown mode, reset to STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_unknown_mode);
#endif /* UART_TERM */
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
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
	if(u8_tanashin_target_mode!=TTR_TANA_MODE_STOP)
	{
		// Mechanism is in active mode, user wants another mode, set target as STOP, it's the only way to transition to another mode.
		// New target mode will apply in the next run of the [transport_state_machine()].
		u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
	}
	else
	{
		// Mechanism is in STOP mode, simple: set target to what user wants.
		// New target mode will apply in the next run of the [transport_state_machine()].
		u8_tanashin_target_mode = mech_tanashin_user_to_transport((*usr_mode));
	}
}

//-------------------------------------- Control mechanism in static mode (not transitioning between modes).
void mech_tanashin_static_mode(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	if(u8_tanashin_mode==TTR_TANA_MODE_STOP)
	{
		// Transport supposed to be in STOP.
		// Check mechanism for mechanical STOP condition.
		if((in_sws&TTR_SW_STOP)==0)
		{
			// Transport is not in STOP mode.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_stop_active); UART_add_flash_string((uint8_t *)cch_force_stop);
#endif /* UART_TERM */
			u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;
			// Force STOP if transport is not in STOP.
			u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;				// Load maximum delay to allow mechanism to revert to STOP (before retrying)
			u8_tanashin_mode = TIM_TANA_DLY_WAIT_REW_ACT;		// Set mode to trigger solenoid
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;			// Set target to be STOP
		}
	}
	else if(u8_tanashin_mode==TTR_TANA_MODE_PB_FWD)
	{
		// Transport supposed to be in PLAYBACK.
		// Check tachometer timer.
		if((*tacho)>TACHO_TANA_PLAY_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
			// Perform auto-stop.
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
		// Check if tape has dropped out.
		else if((in_sws&TTR_SW_TAPE_IN)==0)
		{
#ifdef UART_TERM
			if((u8_tanashin_target_mode!=TTR_TANA_MODE_STOP)||((*usr_mode)!=USR_MODE_STOP))
			{
				UART_add_flash_string((uint8_t *)cch_no_tape);
			}
#endif /* UART_TERM */
			// Tape is out, clear any active mode.
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
		// Check if somehow (manually?) transport switched into STOP.
		else if((in_sws&TTR_SW_STOP)!=0)
		{
			// Mechanism unexpectedly slipped into STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_stop_corr);
#endif /* UART_TERM */
			// Correct logic mode.
			u8_tanashin_mode = TTR_TANA_MODE_STOP;
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
	else if((u8_tanashin_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_mode==TTR_TANA_MODE_FW_REV))
	{
		// Transport supposed to be in FAST WIND.
		// Check tachometer timer.
		if((*tacho)>TACHO_TANA_FWIND_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
			// Perform auto-stop.
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
		// Check if tape has dropped out.
		else if((in_sws&TTR_SW_TAPE_IN)==0)
		{
#ifdef UART_TERM
			if((u8_tanashin_target_mode!=TTR_TANA_MODE_STOP)||((*usr_mode)!=USR_MODE_STOP))
			{
				UART_add_flash_string((uint8_t *)cch_no_tape);
			}
#endif /* UART_TERM */
			// Tape is out, clear any active mode.
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
		// Check if somehow (manually?) transport switched into STOP.
		else if((in_sws&TTR_SW_STOP)!=0)
		{
			// Mechanism unexpectedly slipped into STOP.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_stop_corr);
#endif /* UART_TERM */
			// Correct logic mode.
			u8_tanashin_mode = TTR_TANA_MODE_STOP;
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
			// Clear user mode.
			(*usr_mode) = USR_MODE_STOP;
		}
	}
}

//-------------------------------------- Perform tape transport state machine for Tanashin-clone tape mech.
void mech_tanashin_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	/*sprintf(u8a_buf, "MODE|>%03u<|%01u>%01u>%01u|%02x\n\r",
	u8_tanashin_trans_timer, (*usr_mode), u8_tanashin_target_mode, u8_tanashin_mode, in_sws);
	UART_add_string(u8a_buf);*/
	// Mode overflow protection.
	if((u8_tanashin_mode>=TTR_TANA_MODE_MAX)||(u8_tanashin_target_mode>=TTR_TANA_MODE_MAX))
	{
		// Register logic error.
#ifdef UART_TERM
		UART_add_flash_string((uint8_t *)cch_halt_stop3);
		UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
		u8_tanashin_mode = TTR_TANA_MODE_HALT;
		u8_tanashin_error += TTR_ERR_LOGIC_FAULT;
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
		if(u8_tanashin_target_mode!=TTR_TANA_MODE_HALT)
		{
			// Tape is out, clear any active mode.
			u8_tanashin_target_mode = TTR_TANA_MODE_STOP;
		}
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
			mech_tanashin_static_mode(in_features, in_sws, tacho, usr_mode, play_dir);
		}
	}
	else
	{
		// Transport in transitioning through states.
		// Count down mode transition timer.
		u8_tanashin_trans_timer--;
		// Check if transition is finished.
		if(u8_tanashin_trans_timer==0)
		{
			// Desired mode reached.
			// De-energize solenoid.
			SOLENOID_OFF;
			// Check if transport is not in error.
			if(u8_tanashin_mode!=TTR_TANA_MODE_HALT)
			{
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_mode_done); UART_add_string(" > "); mech_tanashin_UART_dump_mode(u8_tanashin_target_mode); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
				// Save reached state.
				u8_tanashin_mode = u8_tanashin_target_mode;
				// Check if mechanism successfully reached target logic state.
				// Check if target was one of the active modes.
				if((u8_tanashin_target_mode==TTR_TANA_MODE_PB_FWD)
					||(u8_tanashin_target_mode==TTR_TANA_MODE_FW_FWD)||(u8_tanashin_target_mode==TTR_TANA_MODE_FW_REV))
				{
					// Check if mechanical STOP state wasn't cleared.
					if((in_sws&TTR_SW_STOP)!=0)
					{
						// Mechanically mode didn't change from STOP, register an error.
#ifdef UART_TERM
						UART_add_flash_string((uint8_t *)cch_active_stop); UART_add_flash_string((uint8_t *)cch_endl);
						UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
#endif /* UART_TERM */
						u8_tanashin_mode = TTR_TANA_MODE_HALT;
						u8_tanashin_error += TTR_ERR_NO_CTRL;
					}
				}
			}
			// Reset tachometer timer;
			(*tacho) = 0;
		}
		else
		{
			// Still in transition (counter != 0).
			// TODO
			if((u8_tanashin_target_mode==TTR_TANA_SUBMODE_TO_STOP)||(u8_tanashin_target_mode==TTR_TANA_MODE_TO_HALT))
			{
				if(u8_tanashin_trans_timer>(TIM_TANA_DELAY_STOP-TIM_TANA_DLY_SW_ACT))
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
			else if(u8_tanashin_target_mode==TTR_TANA_MODE_PB_FWD)
			{
				if(u8_tanashin_trans_timer>(TIM_TANA_DLY_PB_WAIT-TIM_TANA_DLY_SW_ACT))
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
			else if(u8_tanashin_target_mode==TTR_TANA_MODE_FW_FWD)
			{

			}
			else if(u8_tanashin_target_mode==TTR_TANA_MODE_FW_REV)
			{

			}
		}
	}
#ifdef UART_TERM
	if((u8_tanashin_trans_timer>0)||(mech_tanashin_user_to_transport((*usr_mode))!=u8_tanashin_target_mode)||(u8_tanashin_target_mode!=u8_tanashin_mode))
	{
		sprintf(u8a_buf, "MODE|>%03u<|%01u>%01u>%01u|%02x\n\r",
				u8_tanashin_trans_timer, (*usr_mode), u8_tanashin_target_mode, u8_tanashin_mode, in_sws);
		UART_add_string(u8a_buf);
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

//-------------------------------------- Print Tanashin transport mode alias.
void mech_tanashin_UART_dump_mode(uint8_t in_mode)
{
#ifdef UART_TERM
	if(in_mode==TTR_TANA_MODE_TO_INIT)
	{
		UART_add_flash_string((uint8_t *)cch_mode_to_init);
	}
	else if(in_mode==TTR_TANA_MODE_INIT)
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
#endif /* UART_TERM */
}
