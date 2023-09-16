/**************************************************************************************************************************************************************
calc_crc.h

Copyright © 2023 Maksim Kryukov <fagear@mail.ru>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Created: 2010-09-07

CRC8 table-based calculation module for AVR MCUs and AtmelStudio/AVRStudio/WinAVR/avr-gcc compilers.

**************************************************************************************************************************************************************/

#ifndef CALC_CRC_H_
#define CALC_CRC_H_

#include <avr/pgmspace.h>
#include "config.h"

uint8_t CRC8_init(void);
uint8_t CRC8_calc(uint8_t, uint8_t);

#endif /* CALC_CRC_H_ */
