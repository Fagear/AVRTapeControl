#include "common_log.h"
#include "drv_io.h"
#include "strings.h"

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
	else if(in_mode==USR_MODE_REC_FWD)
	{
		UART_add_flash_string((uint8_t *)cch_mode_rc_fwd);
	}
	else if(in_mode==USR_MODE_REC_REV)
	{
		UART_add_flash_string((uint8_t *)cch_mode_rc_rev);
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

