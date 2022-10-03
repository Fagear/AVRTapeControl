/*
 * drv_uart.h
 *
 * Created:			2009-12-20
 * Modified:		2021-04-13
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		UART driver for AVR MCUs and AVRStudio/WinAVR/AtmelStudio compilers.
 *					The driver is buffer-based, both on transmit and receive sides.
 *					The driver is targeted for real-time systems with no wait loops inside it.
 *					Buffer length is configurable via defines [UART_IN_LEN] and [UART_OUT_LEN].
 *					The driver can set UART speed on-the-fly, using [UART_BAUD_xxxx] defines in [UART_set_speed()]
 *					and [F_CPU] define for CPU clock (in Hz).
 * Supported MCUs:	ATmega32, ATmega32A, ATmega168PA, ATmega328, ATmega328P.
 *				
 */

#ifndef FR_DRV_UART
#define FR_DRV_UART

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include "config.h"		// Contains [UART_IN_LEN], [UART_OUT_LEN] and [UART_EN_OVWR].
#include "drv_cpu.h"	// Contains [F_CPU].

// Speed defines (for use in [UART_set_speed()]).
#define UART_BAUD_9600			(F_CPU/9600/8-1)
#define UART_BAUD_57600			(F_CPU/57600/8-1)
#define UART_BAUD_250k			(F_CPU/250000/8-1)
#define UART_BAUD_500k			(F_CPU/500000/8-1)
#define UART_BAUD_1M			(F_CPU/1000000/8-1)

// Buffer sizes.
#define UART_INPUT_BUF_LEN		UART_IN_LEN		// For receiving buffer.
#define UART_OUTPUT_BUF_LEN		UART_OUT_LEN	// For transmitting buffer.

#if UART_INPUT_BUF_LEN>65535U
	#error Buffer size more than 65535 (16-bit) is not supported! (UART_INPUT_BUF_LEN)
#endif
#if UART_OUTPUT_BUF_LEN>65535U
	#error Buffer size more than 65535 (16-bit) is not supported! (UART_OUTPUT_BUF_LEN)
#endif

// Input string type.
#define UART_CHAR		0	// char* from RAM
#define UART_ROM		1	// uint8_t* from ROM

// Hardware-specific register and buts.
#if SIGNATURE_2 == 0x02	// ATmega32(A)
#define UART_RX_INT			USART_RXC_vect
#define UART_TX_INT			USART_TXC_vect
#define UART_CONF1_REG		UCSRA
#define UART_CONF2_REG		UCSRB
#define UART_SPD_H_REG		UBRRH
#define UART_SPD_L_REG		UBRRL
#define UART_DATA_REG		UDR
#define UART_STATE_REG		UCSRA
#define UART_DBL_SPEED		(1<<U2X)
#define UART_RX_EN			(1<<RXEN)
#define UART_TX_EN			(1<<TXEN)
#define UART_RX_INT_EN		(1<<RXCIE)
#define UART_TX_INT_EN		(1<<TXCIE)
#define UART_RX_COMPLETE	(1<<RXC)
#define UART_DATA_EMPTY		(1<<UDRE)
#define UART_FRAME_ERR		(1<<FE)
#define UART_PARITY_ERR		(1<<UPE)
#define UART_DATA_OVERRUN	(1<<DOR)
#define UART_MODE_8N1		UCSRC=(1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0)
#endif	/*SIGNATURE_2*/

#if SIGNATURE_2 == 0x0B	// ATmega168PA
#define UART_RX_INT			USART_RX_vect
#define UART_TX_INT			USART_TX_vect
#define UART_CONF1_REG		UCSR0A
#define UART_CONF2_REG		UCSR0B
#define UART_CONF3_REG		UCSR0C
#define UART_SPD_H_REG		UBRR0H
#define UART_SPD_L_REG		UBRR0L
#define UART_DATA_REG		UDR0
#define UART_STATE_REG		UCSR0A
#define UART_DBL_SPEED		(1<<U2X0)
#define UART_RX_EN			(1<<RXEN0)
#define UART_TX_EN			(1<<TXEN0)
#define UART_RX_INT_EN		(1<<RXCIE0)
#define UART_TX_INT_EN		(1<<TXCIE0)
#define UART_RX_COMPLETE	(1<<RXC0)
#define UART_DATA_EMPTY		(1<<UDRE0)
#define UART_FRAME_ERR		(1<<FE0)
#define UART_PARITY_ERR		(1<<UPE0)
#define UART_DATA_OVERRUN	(1<<DOR0)
#define UART_MODE_8N1		UCSR0C=(1<<UCSZ01)|(1<<UCSZ00)
#endif	/*SIGNATURE_2*/

#if SIGNATURE_2 == 0x0F	// ATmega328P
#define UART_RX_INT			USART_RX_vect
#define UART_TX_INT			USART_TX_vect
#define UART_CONF1_REG		UCSR0A
#define UART_CONF2_REG		UCSR0B
#define UART_CONF3_REG		UCSR0C
#define UART_SPD_H_REG		UBRR0H
#define UART_SPD_L_REG		UBRR0L
#define UART_DATA_REG		UDR0
#define UART_STATE_REG		UCSR0A
#define UART_DBL_SPEED		(1<<U2X0)
#define UART_RX_EN			(1<<RXEN0)
#define UART_TX_EN			(1<<TXEN0)
#define UART_RX_INT_EN		(1<<RXCIE0)
#define UART_TX_INT_EN		(1<<TXCIE0)
#define UART_RX_COMPLETE	(1<<RXC0)
#define UART_DATA_EMPTY		(1<<UDRE0)
#define UART_FRAME_ERR		(1<<FE0)
#define UART_PARITY_ERR		(1<<UPE0)
#define UART_DATA_OVERRUN	(1<<DOR0)
#define UART_MODE_8N1		UCSR0C=(1<<UCSZ01)|(1<<UCSZ00)
#endif	/*SIGNATURE_2*/

#if SIGNATURE_2 == 0x14	// ATmega328
#define UART_RX_INT			USART_RX_vect
#define UART_TX_INT			USART_TX_vect
#define UART_CONF1_REG		UCSR0A
#define UART_CONF2_REG		UCSR0B
#define UART_CONF3_REG		UCSR0C
#define UART_SPD_H_REG		UBRR0H
#define UART_SPD_L_REG		UBRR0L
#define UART_DATA_REG		UDR0
#define UART_STATE_REG		UCSR0A
#define UART_DBL_SPEED		(1<<U2X0)
#define UART_RX_EN			(1<<RXEN0)
#define UART_TX_EN			(1<<TXEN0)
#define UART_RX_INT_EN		(1<<RXCIE0)
#define UART_TX_INT_EN		(1<<TXCIE0)
#define UART_RX_COMPLETE	(1<<RXC0)
#define UART_DATA_EMPTY		(1<<UDRE0)
#define UART_FRAME_ERR		(1<<FE0)
#define UART_PARITY_ERR		(1<<UPE0)
#define UART_DATA_OVERRUN	(1<<DOR0)
#define UART_MODE_8N1		UCSR0C=(1<<UCSZ01)|(1<<UCSZ00)
#endif	/*SIGNATURE_2*/

// Interrupt header and footer if [ISR_NAKED] is used.
#define INTR_UART_IN	asm volatile("push	r1\npush	r0\nin	r0, 0x3f\npush	r0\neor	r1, r1\npush	r24\npush	r25\npush	r30\npush	r31\n")
#define INTR_UART_OUT	asm volatile("pop	r31\npop	r30\npop	r25\npop	r24\npop	r0\nout	0x3f, r0\npop	r0\npop	r1")

// Allow transmitting buffer to be flushed and overwritten if overflow occurs.
//#define UART_EN_OVWR	1	

void UART_set_speed(uint16_t);					// Set UART speed with "UART_BAUD_xxxx" defines.
void UART_enable(void);							// Enable UART hardware.
void UART_disable(void);						// Disable UART hardware.
void UART_add_string(const char*);				// Add char* string into transmitting buffer (buffer length in [UART_OUTPUT_BUF_LEN]).
void UART_add_flash_string(const uint8_t*);		// Add string from PROGMEM into transmitting buffer (buffer length in [UART_OUTPUT_BUF_LEN]).
void UART_send_byte(void);						// Transmit on byte from transmitting buffer to UART.
void UART_receive_byte(void);					// Receive on byte from UART and put it into receiving buffer (buffer length in [UART_INPUT_BUF_LEN]).
int8_t UART_get_byte(void);						// Read on byte from receiving buffer.
uint16_t UART_get_received_number(void);		// Get number of unread bytes in receiving buffer.
uint16_t UART_get_sending_number(void);			// Get number of not transmitted bytes in transmitting buffer.
void UART_flush_in(void);						// Clear out receiving buffer.
void UART_dump_out(void);						// Dump all bytes one-by-one from transmitting buffer to UART (clear space in transmitting buffer).

#endif /* FR_DRV_UART */
