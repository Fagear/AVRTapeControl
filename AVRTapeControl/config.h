/**************************************************************************************************************************************************************
config.h

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

Created: 2021-04-13

Part of the [AVRTapeControl] project.
Configuration file.
Defines/switches for configuring compile-time options.

**************************************************************************************************************************************************************/

#ifndef CONFIG_H_
#define CONFIG_H_

#include "common_log.h"

// Transport support.
#define SUPP_TANASHIN_MECH			// Tanashin-clone
#define SUPP_CRP42602Y_MECH			// CRP42602Y mechanism from AliExpress (LG-like)
//#define SUPP_KENWOOD_MECH			// Kenwood mechanism

// Data saving into EEPROM
#define USE_EEPROM					// Enable usage of EEPROM for settings
#define CRC8_ROM_DATA				// Put CRC table into ROM instead of RAM.
#define SETTINGS_SIZE		5		// Number of bytes for full [settings_data] union
// Set target size of saving/restoring block for EEPROM driver.
#define EEPROM_TARGET_SIZE	SETTINGS_SIZE
//#define EN_STAT_EEPROM				// Save usage stats to EEPROM

// UART console stuff.
#define UART_IN_LEN			8		// UART receiving buffer length
#define UART_OUT_LEN		512		// UART transmitting buffer length
#define UART_SPEED			UART_BAUD_500k
//#define UART_TERM					// Enable UART debug output (slows down execution and takes up ROM and RAM).

// Default feature sets (described in [common_log.h]).
#define TTR_FEA_DEFAULT				(TTR_FEA_REV_ENABLE)	// Default transport feature settings
#define SRV_FEA_DEFAULT				(/*SRV_FEA_TWO_PLAYS|*//*SRV_FEA_ONE2REC|*/SRV_FEA_PB_AUTOREV/*|SRV_FEA_PB_LOOP|SRV_FEA_PBF2REW|SRV_FEA_FF2REW*/)		// Default service feature settings

//#define DBG_ACT_MON					// Output mode transition activity instead of "record" and "mute" outputs.

#endif /* CONFIG_H_ */
