#include "drv_UART.h"
#include <stdio.h>

#ifdef UART_TERM

static volatile uint16_t p_send=0;		// Points to first symbol for transmitting to UART (TX).
static volatile uint16_t p_write=0;		// Points to next free cell inside TX buffer.
static volatile uint16_t p_receive=0;	// Points to next free cell inside RX buffer from UART.
static volatile uint16_t p_read=0;		// Points to first unread symbol inside RX buffer.
static uint16_t send_char_count=0;
static volatile uint16_t receive_char_count=0;
static uint8_t c_send_arr[UART_OUTPUT_BUF_LEN], c_receive_arr[UART_INPUT_BUF_LEN];

//-------------------------------------- Set UART speed.
void UART_set_speed(uint16_t set_speed)
{
	UART_SPD_L_REG=(uint8_t)(set_speed&0xFF);
	UART_SPD_H_REG=(uint8_t)((set_speed>>8)&0xFF);
}

//-------------------------------------- UART initialization and start.
void UART_enable(void)
{
	// Enable double speed (for more precise speed setting).
	UART_CONF1_REG=UART_DBL_SPEED;
	// Enable receiver and transmitter, interrupt on TX and RX complete.
	UART_CONF2_REG=UART_RX_EN|UART_TX_EN|UART_RX_INT_EN|UART_TX_INT_EN;
	// Set frame format (async, no parity, 1 stop bit, 8 data bits).
	UART_MODE_8N1;
}

//-------------------------------------- UART disable.
void UART_disable(void)
{
	// Disable receiver and transmitter, interrupt on TX and RX complete.
	UART_CONF2_REG&=~(UART_RX_EN|UART_TX_EN|UART_RX_INT_EN|UART_TX_INT_EN);
}

//-------------------------------------- Add string to output buffer.
void add_str_to_out_buf(const uint8_t *input_ptr, const uint8_t data_mode)
{
	volatile uint16_t available, length;
	uint16_t i;
	uint8_t read_byte;
	// Correct error if required.
	//if(send_char_count>UART_OUTPUT_BUF_LEN) send_char_count=UART_OUTPUT_BUF_LEN;
	// Calculate available bytes in buffer.
	available=UART_OUTPUT_BUF_LEN-send_char_count;
	// Reset variables.
	i=0;
	length=0;
	read_byte=1;
	// Find string length.
	while(read_byte!='\0')
	{
		if(data_mode==UART_ROM)
		{
			// Copy byte from ROM.
			read_byte=pgm_read_byte_near(input_ptr+length);
		}
		else
		{
			// Copy byte from RAM.
			read_byte=input_ptr[length];
		}
		// Increase string length.
		length++;
		// Truncate string to buffer length.
		if(length>UART_OUTPUT_BUF_LEN) break;
	}
	// Correct offset.
	length--;
#ifdef UART_EN_OVWR
	// Check available space.
	if(length>available)
	{
		// Reset buffer.
		p_send=0;
		p_write=3;
		send_char_count=3;
		c_send_arr[0]='†';
		c_send_arr[1]='\n';
		c_send_arr[2]='\r';
	}
#else
	// Check available space.
	if(length>available)
	{
		// Do not overfill the buffer.
		return;
	}
#endif /*UART_EN_OVWR*/
	// Fill the buffer.
	while(i<length)
	{
		if(data_mode==UART_ROM)
		{
			// Copy byte from ROM.
			c_send_arr[p_write]=pgm_read_byte_near(input_ptr+i);
		}
		else
		{
			// Copy byte from RAM.
			c_send_arr[p_write]=input_ptr[i];
		}
		// Move pointer.
		p_write++;
		// Loop within buffer.
		if(p_write>=UART_OUTPUT_BUF_LEN) p_write=0;
		// Increase sending data counter.
		i++;
		send_char_count++;
	}
}

//-------------------------------------- Add string to output buffer.
void UART_add_string(const char *u8_input)
{
	add_str_to_out_buf((uint8_t*)u8_input, UART_CHAR);
}

//-------------------------------------- Add string from flash to output buffer.
void UART_add_flash_string(const uint8_t *u8_input)
{
	add_str_to_out_buf((uint8_t*)u8_input, UART_ROM);
}

//-------------------------------------- Send one byte from output buffer to UART.
void UART_send_byte(void)
{
	// Check number of bytes in buffer.
	if(send_char_count>0)
	{
		// Wait for USART register to empty.
		if((UART_STATE_REG&UART_DATA_EMPTY)!=0)
		{
			// Put data into USART register.
			UART_DATA_REG=c_send_arr[p_send];
			// Clear byte (for simulation).
			c_send_arr[p_send]=0;
			// Move pointer.
			p_send++;
			// Loop within buffer.
			if(p_send>=UART_OUTPUT_BUF_LEN) p_send=0;
			// Decrease number of sending bytes.
			send_char_count--;
		}
	}
}

//-------------------------------------- Copy received byte from UART to input buffer.
inline void UART_receive_byte(void)
{
	// Run time:        ***5.000us***.
	volatile uint8_t u8_status, u8_data;
	// Check status register.
	u8_status=UART_STATE_REG;
	u8_data=UART_DATA_REG;
	if((u8_status&(UART_FRAME_ERR|UART_DATA_OVERRUN|UART_PARITY_ERR))==0)
	{
		// Copy received byte if there is room available.
		if(receive_char_count<UART_INPUT_BUF_LEN)
		{
			c_receive_arr[p_receive]=u8_data;
			// Move pointer.
			p_receive++;
			// Loop within buffer.
			if(p_receive>=UART_INPUT_BUF_LEN) p_receive=0;
			// Increase number of received bytes.
			receive_char_count++;
		}
	}
	else
	{
		// Flush receive register.
		while(UART_STATE_REG&UART_RX_COMPLETE) u8_data=UART_DATA_REG;
	}
}

//-------------------------------------- Get first byte from input buffer.
int8_t UART_get_byte(void)
{
	uint8_t u8_output;
	cli();
	if(receive_char_count>0)
	{
		receive_char_count--;
		u8_output=c_receive_arr[p_read];
		sei();
		p_read++;
		if(p_read>=UART_INPUT_BUF_LEN)
		{
			p_read=0;
		}
		return u8_output;
	}
	sei();
	return 0;
}

//-------------------------------------- Get received bytes count in buffer.
uint16_t UART_get_received_number(void)
{
	uint8_t ret_val;
	cli();
	ret_val=receive_char_count;
	sei();
	return ret_val;
}

//-------------------------------------- Get bytes count in output buffer.
uint16_t UART_get_sending_number(void)
{
	return send_char_count;
}

//-------------------------------------- Clear all data from USART input buffer.
void UART_flush_in(void)
{
	uint8_t u8_data;
	while(UART_STATE_REG&UART_RX_COMPLETE) u8_data=UART_DATA_REG;
	cli();
	p_receive=0;
	p_read=0;
	receive_char_count=0;
	sei();
}

//-------------------------------------- Dump all data from output buffer to USART.
void UART_dump_out(void)
{
	// Send all bytes one-by-one until queue is empty.
	while(send_char_count>0)
	{
		UART_send_byte();
		// Reset Watch-Dog timer.
		wdt_reset();
	}
}

#endif /* UART_TERM */
