/*
 * config.h
 *
 * Created:			2021-04-13 14:49:47
 * Modified:		2021-04-13
 * Author:			Maksim Kryukov aka Fagear (fagear@mail.ru)
 * Description:		Configuration for drivers
 *
 */


#ifndef CONFIG_H_
#define CONFIG_H_

// Put CRC table into ROM instead of RAM.
#define CRC8_ROM_DATA

// Enable UART debug output (slows down execution and takes up ROM and RAM).
#define UART_TERM

#define SETTINGS_SIZE		11	// Number of bytes for full [settings_data] union.

#define UART_IN_LEN			8		// UART receiving buffer length.
#define UART_OUT_LEN		768		// UART transmitting buffer length.
#define UART_SPEED			UART_BAUD_125k

// Set target size of saving/restoring block for EEPROM driver.
#define EEPROM_TARGET_SIZE	SETTINGS_SIZE

#endif /* CONFIG_H_ */