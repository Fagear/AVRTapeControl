/*
 * common_log.c
 *
 * Created: 07.09.2023 17:14:47
 *  Author: kryukov
 */

#include "common_log.h"

//-------------------------------------- Print user mode alias.
void UART_dump_user_mode(uint8_t in_mode)
{
#ifdef UART_TERM
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
#endif /* UART_TERM */
}

void UART_dump_settings(uint16_t in_settings)
{
#ifdef UART_TERM
	if((in_settings&TTR_FEA_REV_ENABLE)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_reverse); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_reverse); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((in_settings&TTR_FEA_PB_AUTOREV)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_ab); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_ab); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((in_settings&TTR_FEA_PB_LOOP)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_loop); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_reverse_loop); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((in_settings&TTR_FEA_END_REW)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_rewind); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_auto_rewind); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((in_settings&TTR_FEA_STOP_TACHO)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_tacho_stop); UART_add_flash_string((uint8_t *)cch_enabled);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_tacho_stop); UART_add_flash_string((uint8_t *)cch_disabled);
	}
	if((in_settings&TTR_FEA_TWO_PLAYS)!=0)
	{
		UART_add_flash_string((uint8_t *)cch_set_pb_btns); UART_add_flash_string((uint8_t *)cch_two);
	}
	else
	{
		UART_add_flash_string((uint8_t *)cch_set_pb_btns); UART_add_flash_string((uint8_t *)cch_one);
	}
#endif /* UART_TERM */
}
