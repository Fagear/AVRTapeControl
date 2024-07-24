/**************************************************************************************************************************************************************
drv_spi.h

Copyright © 2024 Maksim Kryukov <fagear@mail.ru>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Created: 2016-09-26

SPI driver for AVR MCUs and AtmelStudio/AVRStudio/WinAVR/avr-gcc compilers.

**************************************************************************************************************************************************************/

#ifndef DRV_SPI_H_
#define DRV_SPI_H_

#include <avr/io.h>

// SPI hardware.
#define SPI_PORT			PORTB
#define SPI_DIR				DDRB
#define SPI_CLK				(1<<5)
#define SPI_MOSI			(1<<3)
#define SPI_MISO			(1<<4)
#define SPI_CS				(1<<2)
#define SPI_INT				SPI_STC_vect	// SPI Serial Transfer Complete
#define SPI_CONTROL			SPCR
#define SPI_STATUS			SPSR
#define SPI_DATA			SPDR
#define SPI_TX_START		SPI_PORT&=~SPI_CS
#define SPI_TX_END			SPI_PORT|=SPI_CS

/*
void SPI_init_master(void);
void SPI_int_enable(void);
void SPI_int_disable(void);
void SPI_send_byte(uint8_t);
uint8_t SPI_read_byte(void);*/

inline void SPI_init_master(void)
{
	// Init pins.
	SPI_PORT&=~(SPI_CLK|SPI_MOSI|SPI_CS);
	SPI_DIR|=SPI_CLK|SPI_MOSI|SPI_CS;
	SPI_DIR&=~SPI_MISO;
	SPI_PORT|=SPI_CS;
		
	// SPI mode 0, clk/4 (250 kHz)
	SPI_CONTROL=(1<<SPE)|(0<<DORD)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(0<<SPR1)|(0<<SPR0);
}

inline void SPI_int_enable(void)
{
	SPI_CONTROL|=(1<<SPIE);
}

/*
inline void SPI_int_disable(void)
{
	SPI_CONTROL&=~(1<<SPIE);
}
*/

inline void SPI_send_byte(uint8_t data)
{
	SPI_DATA=data;
}

/*
inline uint8_t SPI_read_byte(void)
{
	return SPI_DATA;
}
*/

#endif /* DRV_SPI_H_ */