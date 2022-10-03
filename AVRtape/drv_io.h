/*
 * drv_io.h
 *
 * Created:			2021-04-04 19:27:58
 * Modified:		2021-04-13
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		Hardware defines (pseudo-HAL) and setup routines
 *
 */ 

#ifndef DRV_IO_H_
#define DRV_IO_H_

#include <avr/io.h>
#include "drv_spi.h"
#include "drv_uart.h"

// User-input buttons.
#define BTN_PORT_1			PORTC
#define BTN_DIR_1			DDRC
#define BTN_SRC_1			PINC
#define BTN_1				(1<<1)
#define BTN_2				(1<<2)
#define BTN_3				(1<<3)
#define BTN_4				(1<<4)
#define BTN_5				(1<<5)
#define BTN_6				(1<<0)
#define BTN_STOP_STATE		(BTN_SRC_1&BTN_1)	// Stop button
#define BTN_PLAY_STATE		(BTN_SRC_1&BTN_2)	// Play/reverse button
#define BTN_PLAY_REV_STATE	(BTN_SRC_1&BTN_6)	// Play in reverse button
#define BTN_REWD_STATE		(BTN_SRC_1&BTN_3)	// Rewind button
#define BTN_FFWD_STATE		(BTN_SRC_1&BTN_4)	// Fast forward button
#define BTN_REC_STATE		(BTN_SRC_1&BTN_5)	// Record button

// Sensors and switches.
#define SW_PORT				PORTD
#define SW_DIR				DDRD
#define SW_SRC				PIND
#define SW_1				(1<<6)
#define SW_2				(1<<5)
#define SW_3				(1<<4)
#define SW_4				(1<<3)
#define SW_5				(1<<2)
#define SW_TAPE_IN_STATE	(SW_SRC&SW_1)		// Tape is present
#define SW_STOP_STATE		(SW_SRC&SW_2)		// Tape transport in mechanical "STOP" mode
#define SW_TACHO_STATE		(SW_SRC&SW_3)		// Tape pickup tachometer
#define SW_NOREC_FWD_STATE	(SW_SRC&SW_4)		// Rec inhibit in forward direction
#define SW_NOREC_REV_STATE	(SW_SRC&SW_5)		// Rec inhibit in reverse direction

// Record output control.
#define REC_EN_PORT			PORTD
#define REC_EN_DIR			DDRD
#define REC_EN_SRC			PIND
#define REC_EN_BIT			(1<<7)
#define REC_EN_ON			REC_EN_PORT|=REC_EN_BIT
#define REC_EN_OFF			REC_EN_PORT&=~REC_EN_BIT
#define REC_EN_STATE		(REC_EN_SRC&REC_EN_BIT)

// Actuator/solenoid control.
#define SOL_PORT			PORTB
#define SOL_DIR				DDRB
#define SOL_SRC				PINB
#define SOL_BIT				(1<<0)
#define SOLENOID_ON			SOL_PORT|=SOL_BIT
#define SOLENOID_OFF		SOL_PORT&=~SOL_BIT
#define SOLENOID_STATE		(SOL_SRC&SOL_BIT)

// Capstan motor control.
#define CAPSTAN_PORT		PORTB
#define CAPSTAN_DIR			DDRB
#define CAPSTAN_SRC			PINB
#define CAPSTAN_BIT			(1<<1)
#define CAPSTAN_ON			CAPSTAN_PORT|=CAPSTAN_BIT
#define CAPSTAN_OFF			CAPSTAN_PORT&=~CAPSTAN_BIT
#define CAPSTAN_STATE		(CAPSTAN_SRC&CAPSTAN_BIT)

// Service motor control.
#define SMTR_PORT			PORTB
#define SMTR_DIR			DDRB
#define SMTR_SRC			PINB
#define SMTR_BIT1			(1<<0)
#define SMTR_BIT2			(1<<1)
#define SMTR_DIR1_ON		SMTR_PORT|=SMTR_BIT1
#define SMTR_DIR1_OFF		SMTR_PORT&=~SMTR_BIT1
#define SMTR_DIR2_ON		SMTR_PORT|=SMTR_BIT2
#define SMTR_DIR2_OFF		SMTR_PORT&=~SMTR_BIT2

// Watchdog setup.
#define WDT_RESET_DIS		MCUSR&=~(1<<WDRF)
#define WDT_PREP_OFF		WDTCSR|=(1<<WDCE)|(1<<WDE)
#define WDT_SW_OFF			WDTCSR=0x00
#define WDT_FLUSH_REASON	MCUSR=(0<<WDRF)|(0<<BORF)|(0<<EXTRF)|(0<<PORF)
#define WDT_PREP_ON			WDTCSR|=(1<<WDCE)|(1<<WDE)
#define WDT_SW_ON			WDTCSR=(1<<WDE)|(1<<WDP0)|(1<<WDP1)|(1<<WDP2)	// MCU reset after ~2.0 s

// System timer setup.
#define SYST_INT			TIMER2_COMPA_vect			// Interrupt vector alias
#define SYST_CONFIG1		TCCR2A=(1<<WGM21)			// CTC mode (clear on compare with OCR)
#define SYST_CONFIG2		OCR2A=124					// Cycle clock: INclk/(1+124), 1000 Hz cycle
#define SYST_EN_INTR		TIMSK2|=(1<<OCIE2A)			// Enable interrupt
#define SYST_DIS_INTR		TIMSK2&=~(1<<OCIE2A)		// Disable interrupt
#define SYST_START			TCCR2B|=(1<<CS22)			// Start timer with clk/64 clock (125 kHz)
#define SYST_STOP			TCCR2B&=~((1<<CS20)|(1<<CS21)|(1<<CS22))	// Stop timer
#define SYST_DATA_8			TCNT2						// Count register
#define SYST_RESET			SYST_DATA_8=0				// Reset count

// ADC trigger timer setup.
#define ADCT_CONFIG1		TCCR0A=(1<<WGM01)			// CTC mode (clear on compare with OCR)
#define ADCT_CONFIG2		OCR0A=9						// Cycle clock: INclk/(1+9), 100 kHz (4x25 kHz) cycle
#define ADCT_START			TCCR0B|=(1<<CS01)			// Start timer with clk/8 clock (1 MHz)
#define ADCT_STOP			TCCR0B&=~((1<<CS02)|(1<<CS01)|(1<<CS00));	// Stop timer
#define ADCT_DATA_8			TCNT0						// Count register
#define ADCT_RESET			ADCT_DATA_8=0				// Reset count

// ADC setup.
#define ADC_INT				ADC_vect					// Interrupt vector alias
#define ADC_INPUT_SW		ADMUX						// Input mixer register
#define ADC_IN_LEFT_CH		0x00						// Channel bits for "left channel"
#define ADC_IN_RIGHT_CH		0x01						// Channel bits for "right channel"
#define ADC_MUX_MASK		((1<<MUX3)|(1<<MUX2)|(1<<MUX1)|(1<<MUX0))
#define ADC_CONFIG1			ADC_INPUT_SW=(1<<REFS0)|(1<<REFS1)|(1<<ADLAR)|(ADC_IN_LEFT_CH)	// 1.1 V reference, left-adjusted
#define ADC_CONFIG2			ADCSRA=(1<<ADATE)|(1<<ADPS1)						// Auto-trigger, interrupts enabled, clock: clk/4 (2 MHz) within safe 10-bit region (50...200 kHz)
#define ADC_CONFIG3			ADCSRB=(1<<ADTS2)									// Auto-trigger source: Timer 0 Compare Match A
#define ADC_ENABLE			ADCSRA|=(1<<ADEN)			// Enable ADC
#define ADC_EN_INTR			ADCSRA|=(1<<ADIE)			// Enable interrupt
#define ADC_DIS_INTR		ADCSRA&=~(1<<ADIE)			// Disable interrupt
#define ADC_CLR_INTR		ADCSRA|=(1<<ADIF)			// Clear interrupt flag
#define ADC_START			ADCSRA|=(1<<ADSC)			// Start conversion
#define ADC_DATA_16			ADC							// 16-bit data register
#define ADC_DATA_8H			ADCH						// 8-bit high-byte register
#define ADC_DATA_8L			ADCL						// 8-bit low-byte register

// Power consumption optimizations.
#define PWR_COMP_OFF		ACSR|=(1<<ACD)
#define PWR_ADC_OFF			PRR|=(1<<PRADC)
#define PWR_T0_OFF			PRR|=(1<<PRTIM0)
#define PWR_T1_OFF			PRR|=(1<<PRTIM1)
#define PWR_T2_OFF			PRR|=(1<<PRTIM2)
#define PWR_I2C_OFF			PRR|=(1<<PRTWI)
#define PWR_SPI_OFF			PRR|=(1<<PRSPI)
#define PWR_UART_OFF		PRR|=(1<<PRUSART0)
#define PWR_ADC_OPT			DIDR0|=(1<<ADC0D)|(1<<ADC1D)

//-------------------------------------- IO initialization.
inline void HW_init(void)
{
	// Turn off not used devices for power saving.
	PWR_COMP_OFF; PWR_I2C_OFF;
	// Turn off digital buffers on ADC inputs.
	PWR_ADC_OPT;
	
	// Init SPI interface.
	SPI_init_master();
		
	// Init USART interface.
	UART_set_speed(UART_SPEED);
	UART_enable();
		
	// Init ADC.
	/*ADC_ENABLE;
	ADC_CONFIG1;
	ADC_CONFIG2;
	ADC_CONFIG3;
	ADCT_CONFIG1;
	ADCT_CONFIG2;
	ADCT_RESET;
	ADCT_START;
	ADC_EN_INTR;*/
	//TIMSK0|=(1<<OCIE0A);
	
	// Init power output control.
	SOL_PORT&=~SOL_BIT;						// Disable pull-ups/set output to "0"
	SOL_DIR|=SOL_BIT;						// Set pin as output
	CAPSTAN_PORT&=~CAPSTAN_BIT;				// Disable pull-ups/set output to "0"
	CAPSTAN_DIR|=CAPSTAN_BIT;				// Set pin as output
	SMTR_PORT&=~(SMTR_BIT1|SMTR_BIT2);		// Disable pull-ups/set output to "0"
	SMTR_DIR|=(SMTR_BIT1|SMTR_BIT2);		// Set pins as outputs
	
	// Init record control.
	REC_EN_PORT&=~(REC_EN_BIT);				// Disable pull-ups/set output to "0"
	REC_EN_DIR|=REC_EN_BIT;					// Set pins as outputs
	
	// Init user-buttons.
	BTN_DIR_1&=~(BTN_1|BTN_2|BTN_3|BTN_4|BTN_5|BTN_6);	// Set pins as inputs
	BTN_PORT_1|=BTN_1|BTN_2|BTN_3|BTN_4|BTN_5|BTN_6;	// Turn on pull-ups
		
	SW_DIR&=~(SW_1|SW_2|SW_3|SW_4|SW_5);	// Set pins as inputs
	SW_PORT|=SW_1|SW_2|SW_3|SW_4|SW_5;		// Turn on pull-ups
	
	// System timing.
	SYST_CONFIG1;
	SYST_CONFIG2;
	SYST_RESET;
	SYST_EN_INTR;
	
	// Timer for PWM output.
	/*OCR1AL = 128;
	TCNT1 = 0;
	TCCR1A = (1<<WGM10)|(1<<COM1A1)|(1<<COM1A0);	// Phase-correct 8-bit PWM
	TCCR1B |= (1<<CS11);					// clk/8
	*/
	// Enable ADC interrupts.
	//ADC_EN_INTR;
}

#endif /* DRV_IO_H_ */