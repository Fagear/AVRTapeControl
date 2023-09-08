#include "mech_tanashin.h"

uint8_t u8_tanashin_target_mode=TTR_TANA_MODE_TO_INIT;	// Target transport mode (derived from [usr_mode])
uint8_t u8_tanashin_mode=TTR_TANA_MODE_STOP;		// Current tape transport mode (transitions to [u8_tanashin_target_mode])
uint8_t u8_tanashin_error=TTR_ERR_NONE;				// Last transport error
uint8_t u8_tanashin_trans_timer=0;					// Solenoid holding timer
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

//-------------------------------------- Perform tape transport state machine for Tanashin-clone tape mech.
void mech_tanashin_state_machine(uint16_t in_features, uint8_t in_sws, uint8_t *tacho, uint8_t *usr_mode, uint8_t *play_dir)
{
	/*sprintf(u8a_buf, "MODE|>%03u<|%01u>%01u>%01u|%02x\n\r",
	u8_tanashin_trans_timer, (*usr_mode), u8_tanashin_target_mode, u8_tanashin_mode, in_sws);
	UART_add_string(u8a_buf);*/
	// This transport supports only forward playback.
	(*play_dir) = PB_DIR_FWD;

	if(u8_tanashin_trans_timer==0)
	{
		// Transport is in stable state (mode transition finished).
		// Check if transport is in error-state.
		if(u8_tanashin_mode==TTR_TANA_MODE_HALT)
		{
			// Transport control is halted due to an error, ignore any state transitions and user-requests.
			// Set upper levels to the same mode.
			u8_tanashin_target_mode = TTR_TANA_MODE_HALT;
			(*usr_mode) = USR_MODE_STOP;
			// Keep timer reset, no mode transitions.
			u8_tanashin_trans_timer = 0;
			// Keep capstan stopped.
			CAPSTAN_OFF;
			// Keep solenoid inactive.
			SOLENOID_OFF;
			// Check mechanical stop sensor.
			if((in_sws&TTR_SW_STOP)==0)
			{
				// Transport is not in STOP mode.
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_halt_active); UART_add_flash_string((uint8_t *)cch_force_stop);
#endif /* UART_TERM */
				// Force STOP if transport is not in STOP.
				u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;				// Load maximum delay to allow mechanism to revert to STOP (before retrying)
				u8_tanashin_target_mode = TTR_TANA_MODE_TO_HALT;			// Set mode to trigger solenoid
			}
		}
		// Check if transport has to start transitioning to another stable state.
		else if(u8_tanashin_target_mode!=u8_tanashin_mode)
		{
			// Target mode is not the same as current transport mode (need to start transition to another mode).
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_target2current1); mech_tanashin_UART_dump_mode(u8_tanashin_target_mode);
			UART_add_flash_string((uint8_t *)cch_target2current2); mech_tanashin_UART_dump_mode(u8_tanashin_mode); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
			if(u8_tanashin_target_mode==TTR_TANA_MODE_TO_INIT)
			{
				// Target mode: start-up delay.
#ifdef UART_TERM
				UART_add_flash_string((uint8_t *)cch_startup_delay);
#endif /* UART_TERM */
				u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;				// Load maximum delay to allow mechanism to stabilize
				u8_tanashin_mode = TTR_TANA_MODE_INIT;				// Set mode for waiting initialization
				u8_tanashin_target_mode = TTR_TANA_MODE_STOP;			// Set next mode to STOP
				(*usr_mode) = USR_MODE_STOP;						// Reset user mode to STOP
			}
			else if(u8_tanashin_target_mode==TTR_TANA_MODE_STOP)
			{
				// Target mode: full stop.
#ifdef UART_TERM
				UART_add_string("New: stop\n\r");
#endif /* UART_TERM */
				u8_tanashin_trans_timer = TIM_TANA_DELAY_STOP;
				u8_tanashin_mode = TTR_TANA_MODE_TO_STOP;
			}
			else
			{
				// Reset last error.
				u8_tanashin_error = TTR_ERR_NONE;
				// Reset tachometer timer.
				(*tacho) = 0;
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
			}
		}
		// Transport is not in error and is in stable state (target mode reached),
		// check if user requests another mode.
		else if(mech_tanashin_user_to_transport((*usr_mode))!=u8_tanashin_target_mode)
		{
			// Not in disabled state, target mode is reached.
			// User wants another mode than current transport target is.
#ifdef UART_TERM
			UART_add_flash_string((uint8_t *)cch_user2target1); mech_tanashin_UART_dump_mode(u8_tanashin_target_mode);
			UART_add_flash_string((uint8_t *)cch_user2target2); UART_dump_user_mode((*usr_mode)); UART_add_flash_string((uint8_t *)cch_endl);
#endif /* UART_TERM */
			// TODO: switching between modes.
			/*if(u8_tanashin_target_mode!=TTR_42602_MODE_STOP)
			{
				// Mechanism is in active mode, user wants another mode, set target as STOP, it's the only way to transition to another mode.
				// New target mode will apply in the next run of the [transport_state_machine()].
				u8_tanashin_target_mode = TTR_42602_MODE_STOP;
				UART_add_string("New: stop (u)\n\r");
			}
			else
			{
				// Mechanism is in STOP mode, simple: set target to what user wants.
				// New target mode will apply in the next run of the [transport_state_machine()].
				u8_tanashin_target_mode = mech_tanashin_user_to_transport((*usr_mode));
				UART_add_string("New: active (u)\n\r");
			}*/
		}
		else
		{
			// Transport is not due to transition through modes (u8_tanashin_mode == u8_tanashin_target_mode).
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
			if((u8_tanashin_target_mode==TTR_TANA_MODE_TO_STOP)||(u8_tanashin_target_mode==TTR_TANA_MODE_TO_HALT))
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
#endif /* UART_TERM */
}
