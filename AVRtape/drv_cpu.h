/*
 * drv_cpu.h
 *
 * Created:			2009-04-03
 * Modified:		2021-04-13
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		Some CPU-related defines
 *
 */

#ifndef FGR_DRV_CPU_H_
#define FGR_DRV_CPU_H_		1

#define F_CPU		8000000UL 					// MCU core clock: 8 MHz
#define NOP			asm volatile("nop\n")		// Skip a clock.
#define SLEEP		asm volatile("sleep\n")		// Enter sleep mode.

// Simple interrupt header and footer if [ISR_NAKED] is used.
#define INTR_IN		asm volatile("push	r0\nin	r0, 0x3f\npush	r24\n")				
#define INTR_OUT	asm volatile("pop	r24\nout	0x3f, r0\npop	r0\nreti\n")

// Bool defines.
enum
{
	B_FALSE,
	B_TRUE
};

#endif
