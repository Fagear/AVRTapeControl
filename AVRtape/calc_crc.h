/*
 * calc_crc.h
 *
 * Created:			2010-09-07
 * Modified:		2017-10-16
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		CRC8 table-based calculation module for AVR MCUs and AtmelStudio/AVRStudio/WinAVR/avr-gcc compilers.
 *				
 */

#ifndef FGR_CALC_CRC
#define FGR_CALC_CRC

#include <avr/pgmspace.h>
#include "config.h"

uint8_t CRC8_init(void);
uint8_t CRC8_calc(uint8_t, uint8_t);

#endif /* FGR_CALC_CRC */
