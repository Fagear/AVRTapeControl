#include "mech_crp42602y.h"

uint8_t u8_crp42602y_target_mode=TTR_42602_MODE_TO_INIT;// Target transport mode (derived from [usr_mode])
uint8_t u8_crp42602y_mode=TTR_42602_MODE_STOP;      	// Current tape transport mode (transitions to [u8_crp42602y_target_mode])
uint8_t u8_crp42602y_error=TTR_ERR_NONE;				// Last transport error
uint8_t u8_crp42602y_trans_timer=0;						// Solenoid holding timer
char u8a_buf[48];										// Buffer for UART debug messages

//-------------------------------------- Convert user mode to transport mode for CRP42602Y mechanism.
uint8_t mech_crp42602y_user_to_transport(uint8_t in_mode)
{
	if(in_mode==USR_MODE_PLAY_FWD)
	{
		return TTR_42602_MODE_PB_FWD;
	}
	if(in_mode==USR_MODE_PLAY_REV)
	{
		return TTR_42602_MODE_PB_REV;
	}
	if(in_mode==USR_MODE_FWIND_FWD)
	{
		return TTR_42602_MODE_FW_FWD;
	}
	if(in_mode==USR_MODE_FWIND_REV)
	{
		return TTR_42602_MODE_FW_REV;
	}
	return TTR_42602_MODE_STOP;
}

//--------------------------------------
void mech_crp42602y_static_halt(uint8_t in_sws, uint8_t *usr_mode)
{
	// Reinforce halt mode.
	u8_crp42602y_target_mode = TTR_42602_MODE_HALT;
	// Clear user mode.
	(*usr_mode) = USR_MODE_STOP;
	// Keep transition timer reset.
	u8_crp42602y_trans_timer = 0;
	// Keep solenoid inactive.
	SOLENOID_OFF;
	// Check mechanical stop sensor.
	if((in_sws&TTR_SW_STOP)==0)
	{
		// Transport is not in STOP mode.
		UART_add_flash_string((uint8_t *)cch_halt_force_stop);
		// Force STOP if transport is not in STOP.
		u8_crp42602y_trans_timer = TIM_42602_DELAY_STOP;
		// Pull solenoid in to initiate mode change.
		SOLENOID_ON;
	}
}

//-------------------------------------- Start transition from current mode to target mode.
void mech_crp42602y_target2mode(uint8_t *usr_mode)
{
	UART_add_string("TARGET!=CURRENT : "); mech_crp42602y_UART_dump_mode(u8_crp42602y_target_mode); UART_add_string(" != "); mech_crp42602y_UART_dump_mode(u8_crp42602y_mode); UART_add_flash_string((uint8_t *)cch_endl);
	if(u8_crp42602y_target_mode==TTR_42602_MODE_TO_INIT)
	{
		// Target mode: start-up delay.
		UART_add_flash_string((uint8_t *)cch_startup_delay);
		// Set time for waiting.
		u8_crp42602y_trans_timer = TIM_42602_DELAY_STOP;
		// Put transport in init waiting mode.
		u8_crp42602y_mode = TTR_42602_SUBMODE_INIT;
		// Move target to STOP mode.
		u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
		// Clear user mode.
		(*usr_mode) = USR_MODE_STOP;
		SOLENOID_OFF;
	}
	else if(u8_crp42602y_target_mode==TTR_42602_MODE_STOP)
	{
		// Target mode: full stop.
		UART_add_string("New: stop\n\r");
		u8_crp42602y_trans_timer = TIM_42602_DELAY_STOP;
		u8_crp42602y_mode = TTR_42602_SUBMODE_TO_STOP;
		// Pull solenoid in to initiate mode change.
		SOLENOID_ON;
	}
	else
	{
		// Check new target mode.
		if((u8_crp42602y_target_mode>=TTR_42602_MODE_PB_FWD)&&(u8_crp42602y_target_mode<=TTR_42602_MODE_FW_REV_HD_REV))
		{
			// Start transition to active mode.
			UART_add_string("New: active\n\r");
			u8_crp42602y_trans_timer = TIM_42602_DELAY_RUN;
			u8_crp42602y_mode = TTR_42602_SUBMODE_TO_START;
			// Pull solenoid in to initiate mode change.
			SOLENOID_ON;
		}
		else
		{
			// Unknown mode, reset to STOP.
			UART_add_flash_string((uint8_t *)cch_unknown_mode);
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			(*usr_mode) = USR_MODE_STOP;
		}
		// Reset last error.
		u8_crp42602y_error = TTR_ERR_NONE;
	}
}

//-------------------------------------- Take in user desired mode and set new target mode.
void mech_crp42602y_user2target(uint8_t *usr_mode)
{
	UART_add_string("USER!=TARGET : "); UART_dump_user_mode((*usr_mode)); UART_add_string(" != "); mech_crp42602y_UART_dump_mode(u8_crp42602y_target_mode); UART_add_flash_string((uint8_t *)cch_endl);
	if(u8_crp42602y_target_mode!=TTR_42602_MODE_STOP)
	{
		// Mechanism is in active mode, user wants another mode, set target as STOP, it's the only way to transition to another mode.
		// New target mode will apply in the next run of the [mech_crp42602y_state_machine()].
		u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
		UART_add_string("New: stop (u)\n\r");
	}
	else
	{
		// Mechanism is in STOP mode, simple: set target to what user wants.
		u8_crp42602y_target_mode = mech_crp42602y_user_to_transport((*usr_mode));
		UART_add_string("New: active (u)\n\r");
	}
}

//-------------------------------------- Control mechanism in static mode (not transitioning between modes).
void mech_crp42602y_static_mode(uint8_t in_features, uint8_t in_sws, uint8_t *taho, uint8_t *usr_mode, uint8_t *play_dir)
{
	if(u8_crp42602y_mode==TTR_42602_MODE_STOP)
	{
		// Transport supposed to be in STOP.
		// Check mechanism for mechanical STOP condition.
		if((in_sws&TTR_SW_STOP)==0)
		{
			// Transport is not in STOP mode.
			UART_add_flash_string((uint8_t *)cch_stop_active);
			// Force STOP if transport is not in STOP.
			u8_crp42602y_trans_timer = TIM_42602_DELAY_STOP;
			u8_crp42602y_mode = TTR_42602_SUBMODE_TO_STOP;
			// Pull solenoid in to initiate mode change.
			SOLENOID_ON;
		}
		else if((*taho)>TACHO_42602_STOP_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
			UART_add_flash_string((uint8_t *)cch_bad_drive1);
			UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_bad_drive2);
			// No motor drive or bad belts, register an error.
			u8_crp42602y_mode = TTR_42602_MODE_HALT;
			u8_crp42602y_error += TTR_ERR_BAD_DRIVE;
		}
	}
	else if((u8_crp42602y_mode==TTR_42602_MODE_PB_FWD)||(u8_crp42602y_mode==TTR_42602_MODE_PB_REV))
	{
		// Transport supposed to be in PLAYBACK.
		if((*taho)>TACHO_42602_PLAY_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
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
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_fwd_rev);
						// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
						(*usr_mode) = USR_MODE_PLAY_REV;
					}
					else if((u8_crp42602y_mode==TTR_42602_MODE_PB_REV)&&((in_features&TTR_FEA_PB_LOOP)!=0))
					{
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_reverse); UART_add_flash_string((uint8_t *)cch_reverse_rev_fwd);
						// Queue auto-reverse (set user mode to next mode that will be applied after STOP).
						(*usr_mode) = USR_MODE_PLAY_FWD;
					}
					else
					{
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
						// Stop mode already queued.
					}
				}
				else
				{
					// Auto-reverse is disabled.
					if(u8_crp42602y_mode==TTR_42602_MODE_PB_REV)
					{
						// Playback was in reverse direction.
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
						// Make next playback direction in reverse direction after auto-stop.
						(*play_dir) = PB_DIR_REV;
						// Stop mode already queued.
					}
					else if((in_features&TTR_FEA_END_REW)!=0)
					{
						// Playback was in forward direction.
						// Auto-rewind is enabled.
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_rewind);
						// Queue rewind.
						(*usr_mode) = USR_MODE_FWIND_REV;
					}
					else
					{
						// No auto-reverse or auto-rewind.
						UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
						// Stop mode already queued.
					}
				}
			}
			else if((in_features&TTR_FEA_END_REW)!=0)
			{
				// No reverse operations and auto-rewind is enabled.
				UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_rewind);
				// Queue rewind.
				(*usr_mode) = USR_MODE_FWIND_REV;
			}
			else
			{
				// No reverse operations and no auto-rewind.
				UART_add_flash_string((uint8_t *)cch_no_tacho_pb); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
				// Stop mode already queued.
			}
		}
		if((in_sws&TTR_SW_STOP)!=0)
		{
			// Mechanism unexpectedly slipped into STOP.
			UART_add_flash_string((uint8_t *)cch_stop_corr);
			// Correct logic state to correspond with reality.
			u8_crp42602y_mode = TTR_42602_MODE_STOP;
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			(*usr_mode) = USR_MODE_STOP;
			u8_crp42602y_trans_timer = 0;
		}
	}
	else if((u8_crp42602y_mode==TTR_42602_MODE_FW_FWD)||(u8_crp42602y_mode==TTR_42602_MODE_FW_REV)||(u8_crp42602y_mode==TTR_42602_MODE_FW_FWD_HD_REV)||(u8_crp42602y_mode==TTR_42602_MODE_FW_REV_HD_REV))
	{
		// Transport supposed to be in FAST WIND.
		if((*taho)>TACHO_42602_FWIND_DLY_MAX)
		{
			// No signal from takeup tachometer for too long.
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
				if((u8_crp42602y_mode==TTR_42602_MODE_FW_REV)||(u8_crp42602y_mode==TTR_42602_MODE_FW_REV_HD_REV))
				{
					// Fast wind was in reverse direction.
					UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_tape_end);
					// Make next playback direction in reverse direction after auto-stop.
					//(*play_dir) = PB_DIR_REV;
				}
				else if((in_features&TTR_FEA_END_REW)!=0)
				{
					// Fast wind was in forward direction.
					UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_rewind);
					// Auto-rewind is enabled.
					// Set mode to rewind.
					(*usr_mode) = USR_MODE_FWIND_REV;
					// Make next playback direction in reverse direction after auto-reverse.
					(*play_dir) = PB_DIR_REV;
				}
			}
			else
			{
				UART_add_flash_string((uint8_t *)cch_no_tacho_fw); UART_add_flash_string((uint8_t *)cch_auto_stop); UART_add_flash_string((uint8_t *)cch_endl);
			}
		}
		if((in_sws&TTR_SW_STOP)!=0)
		{
			// Mechanism unexpectedly slipped into STOP.
			UART_add_flash_string((uint8_t *)cch_stop_corr);
			// Correct logic mode.
			u8_crp42602y_mode = TTR_42602_MODE_STOP;
			u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
			(*usr_mode) = USR_MODE_STOP;
			u8_crp42602y_trans_timer = 0;
		}
	}
}

//--------------------------------------
void mech_crp42602y_cyclogram()
{
	uint8_t u8_inv_timer;
	if(TIM_42602_DELAY_RUN>=u8_crp42602y_trans_timer)
	{
		// Invert timer direction.
		u8_inv_timer = TIM_42602_DELAY_RUN - u8_crp42602y_trans_timer;
	}
	else
	{
		// Overflow protection.
		u8_inv_timer = 0;
	}
	// Transition stages from last to first (due to timing overlapping).
	if(u8_inv_timer>=TIM_42602_DLY_WAIT_MODE)
	{
		// Cyclogram finished, waiting for transport to reach stable state.
		u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_RUN;
		// Release solenoid.
		SOLENOID_OFF;
	}
	else if(u8_inv_timer>=TIM_42602_DLY_TAKEUP_DIR)
	{
		// Takeup direction range.
		u8_crp42602y_mode = TTR_42602_SUBMODE_TU_DIR_SEL;
		if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)||
			(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD)||
			(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD_HD_REV))
		{
			// Pull solenoid in to select takeup in forward direction.
			SOLENOID_ON;
		}
		else
		{
			// Keep solenoid off for takeup in reverse direction
			SOLENOID_OFF;
		}
	}
	else if(u8_inv_timer>=TIM_42602_DLY_WAIT_TAKEUP)
	{
		// Pinch roller engaged, wait for pickup direction range.
		u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_TAKEUP;
		// Release solenoid entering "gray zone".
		SOLENOID_OFF;
	}
	else if(u8_inv_timer>=TIM_42602_DLY_PINCH_EN)
	{
		// Pinch engage range.
		u8_crp42602y_mode = TTR_42602_SUBMODE_PINCH_SEL;
		if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)||
			(u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV))
		{
			// Pull solenoid in to enable pinch roller.
			SOLENOID_ON;
		}
		else
		{
			// Keep solenoid off to select fast winding.
			SOLENOID_OFF;
		}

	}
	else if(u8_inv_timer>=TIM_42602_DLY_WAIT_PINCH)
	{
		// Pinch direction selection finished, wait for pinch range.
		u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_PINCH;
		// Keep solenoid off for the "gray zone".
		SOLENOID_OFF;
	}
	else if(u8_inv_timer>=TIM_42602_DLY_HEAD_DIR)
	{
		// Pinch/head direction range.
		u8_crp42602y_mode = TTR_42602_SUBMODE_HD_DIR_SEL;
		if((u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV)||
			(u8_crp42602y_target_mode==TTR_42602_MODE_FW_FWD_HD_REV)||
			(u8_crp42602y_target_mode==TTR_42602_MODE_FW_REV_HD_REV))
		{
			// Pull solenoid in for head in reverse direction.
			SOLENOID_ON;
		}
		else
		{
			// Keep solenoid off for head in forward direction.
			SOLENOID_OFF;
		}

	}
	else if(u8_inv_timer>=TIM_42602_DLY_WAIT_HEAD)
	{
		// Mode change started, wait for pinch/head direction selection region.
		u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_DIR;
		// Release solenoid entering "gray zone".
		SOLENOID_OFF;
	}
	sprintf(u8a_buf, "TRS|>%03u<|%01u|%02u|%02u\n\r",
			u8_crp42602y_trans_timer,
			(SOLENOID_STATE==0)?0:1,
			u8_crp42602y_mode,
			u8_crp42602y_target_mode);
	UART_add_string(u8a_buf);
}

//-------------------------------------- Perform tape transport state machine for CRP42602Y tape mech.
void mech_crp42602y_state_machine(uint8_t in_features, uint8_t in_sws, uint8_t *taho, uint8_t *usr_mode, uint8_t *play_dir)
{
	// Mode overflow protection.
	if((u8_crp42602y_mode>=TTR_42602_MODE_MAX)||(u8_crp42602y_target_mode>=TTR_42602_MODE_MAX))
	{
		// Register logic error.
		u8_crp42602y_mode = TTR_42602_MODE_HALT;
		u8_crp42602y_error += TTR_ERR_LOGIC_FAULT;
	}
	// Check if tape is present.
	if((in_sws&TTR_SW_TAPE_IN)==0)
	{
		// Tape is not found.
		// Reset last played direction if tape is removed.
		(*play_dir) = PB_DIR_FWD;
		// Check if transport was in active mode last time.
		if((*usr_mode)!=USR_MODE_STOP)
		{
			// Tape was moving, dump a message about lost tape.
			UART_add_flash_string((uint8_t *)cch_no_tape);
		}
		// Clear user mode.
		(*usr_mode) = USR_MODE_STOP;
		// Tape is out, clear any active mode.
		u8_crp42602y_target_mode = TTR_42602_MODE_STOP;
	}
	// Check if transport mode transition is in progress.
	if(u8_crp42602y_trans_timer==0)
	{
		// Transport is in stable state (mode transition finished).
		// Check if transport opetations are halted.
		if(u8_crp42602y_mode==TTR_42602_MODE_HALT)
		{
			// Transport control is halted.
			mech_crp42602y_static_halt(in_sws, usr_mode);
		}
		// Check if current transport mode is the same as target transport mode.
		else if(u8_crp42602y_target_mode!=u8_crp42602y_mode)
		{
			// Target transport mode is not the same as current transport mode (need to start transition to another mode).
			mech_crp42602y_target2mode(usr_mode);
		}
		// Check if user desired mode differs from transport target.
		else if(mech_crp42602y_user_to_transport((*usr_mode))!=u8_crp42602y_target_mode)
		{
			// Not in disabled state, target mode is reached.
			// User wants another mode than current transport target is.
			mech_crp42602y_user2target(usr_mode);
		}
		else
		{
			// Transport is not due to transition through modes (u8_crp42602y_mode == u8_crp42602y_target_mode).
			mech_crp42602y_static_mode(in_features, in_sws, taho, usr_mode, play_dir);
		}
	}
	else
	{
		// Transport is performing mode transition.
		// Count down mode transition timer.
		u8_crp42602y_trans_timer--;
		// Still in transition (counter != 0).
		if(u8_crp42602y_mode==TTR_42602_MODE_HALT)
		{
			// Desired mode: recovery stop in halt mode.
			if(u8_crp42602y_trans_timer<=(TIM_42602_DELAY_STOP-TIM_42602_DLY_STOP))
			{
				// Initial time for activating mode transition to STOP has passed.
				// Release solenoid while waiting for transport to transition to STOP.
				SOLENOID_OFF;
			}
		}
		else if(u8_crp42602y_mode==TTR_42602_SUBMODE_INIT)
		{
			// Start-up delay.
			// Keep solenoid off.
			SOLENOID_OFF;
		}
		else if(u8_crp42602y_target_mode==TTR_42602_MODE_STOP)
		{
			// Desired mode: stop.
			if(u8_crp42602y_trans_timer<=(TIM_42602_DELAY_STOP-TIM_42602_DLY_STOP))
			{
				// Initial time for activating mode transition to STOP has passed.
				// Wait for transport to transition to STOP.
				u8_crp42602y_mode = TTR_42602_SUBMODE_WAIT_STOP;
				// Release solenoid.
				SOLENOID_OFF;
			}
		}
		else if((u8_crp42602y_target_mode>=TTR_42602_MODE_PB_FWD)&&(u8_crp42602y_target_mode<=TTR_42602_MODE_FW_REV_HD_REV))
		{
			// Mode transition cyclogram.
			mech_crp42602y_cyclogram();
		}
		// Check if transition just finished.
		if(u8_crp42602y_trans_timer==0)
		{
			// Desired mode reached.
			// Release solenoid.
			SOLENOID_OFF;
			if(u8_crp42602y_mode!=TTR_42602_MODE_HALT)
			{
				//sprintf(u8a_buf, "TRFIN|%01u>%01u\n\r", u8_crp42602y_mode, u8_crp42602y_target_mode);
				UART_add_string("Mode transition done: "); mech_crp42602y_UART_dump_mode(u8_crp42602y_mode); UART_add_flash_string((uint8_t *)cch_arrow); mech_crp42602y_UART_dump_mode(u8_crp42602y_target_mode); UART_add_flash_string((uint8_t *)cch_endl);

				// Save new transport state.
				u8_crp42602y_mode = u8_crp42602y_target_mode;
				// Check if mechanism successfully reached target logic state.
				// Check if target was one of the active modes.
				if((u8_crp42602y_target_mode>=TTR_42602_MODE_PB_FWD)&&(u8_crp42602y_target_mode<=TTR_42602_MODE_FW_REV_HD_REV))
				{
					// Check if mechanical STOP state wasn't cleared.
					if((in_sws&TTR_SW_STOP)!=0)
					{
						// Mechanically mode didn't change from STOP, register an error.
						u8_crp42602y_mode = TTR_42602_MODE_HALT;
						u8_crp42602y_error += TTR_ERR_NO_CTRL;
						//UART_add_string("Reached active but STOP is present, halting...\n\r");
						UART_add_flash_string((uint8_t *)cch_halt_stop1);
						UART_add_flash_string((uint8_t *)cch_ttr_halt); UART_add_flash_string((uint8_t *)cch_halt_stop2);
					}
					else
					{
						// Update last playback direction.
						if(u8_crp42602y_target_mode==TTR_42602_MODE_PB_FWD)
						{
							(*play_dir) = PB_DIR_FWD;
						}
						else if(u8_crp42602y_target_mode==TTR_42602_MODE_PB_REV)
						{
							(*play_dir) = PB_DIR_REV;
						}
					}
				}
			}
			// Reset tachometer timer.
			(*taho) = 0;
		}
	}
	// Debug PWM.
	//OCR1AL = (u8_crp42602y_mode+1)*(255/TTR_MODE_MAX);
	/*if((u8_crp42602y_trans_timer>0)||((*usr_mode)!=u8_crp42602y_target_mode)||(u8_crp42602y_target_mode!=u8_crp42602y_mode))
	{
		sprintf(u8a_buf, "MODE|>%03u<|%01u>%01u>%01u\n\r",
				u8_crp42602y_trans_timer, (*usr_mode), u8_crp42602y_target_mode, u8_crp42602y_mode);
		UART_add_string(u8a_buf);
	}*/
	UART_dump_out();
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

//-------------------------------------- Print CRP42602Y transport mode alias.
void mech_crp42602y_UART_dump_mode(uint8_t in_mode)
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
		else if(in_mode==TTR_42602_SUBMODE_TO_START)
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
}
