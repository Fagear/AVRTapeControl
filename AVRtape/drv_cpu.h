/**************************************************************************************************************************************************************
drv_cpu.h

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

Created: 2009-04-03

CPU-related defines: CPU clock and assembler snippets for interrupt handling.
For AVR MCUs and AtmelStudio/AVRStudio/WinAVR/avr-gcc compilers.

**************************************************************************************************************************************************************/

#ifndef DRV_CPU_H_
#define DRV_CPU_H_		1

#define F_CPU		1000000UL 					// MCU core clock: 1 MHz (8 MHz/8)
#define NOP			asm volatile("nop\n")		// Skip a clock.
#define SLEEP		asm volatile("sleep\n")		// Enter sleep mode.

// Simple interrupt header and footer if [ISR_NAKED] is used.
#define INTR_IN		asm volatile("push	r0\nin	r0, 0x3f\npush	r24\n")				
#define INTR_OUT	asm volatile("pop	r24\nout	0x3f, r0\npop	r0\nreti\n")
#define INTR_OUT_S	asm volatile("reti\n")

#endif /* DRV_CPU_H_ */
