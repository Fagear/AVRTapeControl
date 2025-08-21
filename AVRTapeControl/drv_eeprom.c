#include "drv_EEPROM.h"

static uint16_t u16_current_address=0;

// Erase a byte in EEPROM.
// This function turns off interrupts globally and turns them back on by itself!
// [u16_addr] - base EEPROM address;
// [offset] - offset from base address in bytes;
#ifndef EEP_NO_ERASE
#ifdef EEP_16BIT_ADDR
void EEPROM_erase_byte(const uint16_t *u16_addr, uint16_t offset)
#else
void EEPROM_erase_byte(const uint16_t *u16_addr, uint8_t offset)
#endif	/*EEP_16BIT_ADDR*/
{
	// Wait for completion of previous write.
	WAIT_EEP_WR wdt_reset();
	WAIT_SELF_WR wdt_reset();
	// Set programming mode.
	EEP_SET_ER1; EEP_SET_ER2;
	// Set up address and data registers.
	EEP_ADDR_REG=((*u16_addr)+offset);
	// Interrupts MUST BE DISABLED!
	cli();
	// Preparing for EEPROM erasing.
	EEP_PREP_WRITE;
	// Start EEPROM write.
	EEP_START_WRITE;
	// Interrupts can be enabled.
	sei();
}

// Erase a byte in EEPROM.
// This function needs global interrupts to be turned off before call!
// [u16_addr] - base EEPROM address;
// [offset] - offset from base address in bytes;
#ifdef EEP_16BIT_ADDR
void EEPROM_erase_byte_intfree(const uint16_t *u16_addr, uint16_t offset)
#else
void EEPROM_erase_byte_intfree(const uint16_t *u16_addr, uint8_t offset)
#endif	/*EEP_16BIT_ADDR*/
{
	// Wait for completion of previous write.
	WAIT_EEP_WR wdt_reset();
	WAIT_SELF_WR wdt_reset();
	// Set programming mode.
	EEP_SET_ER1; EEP_SET_ER2;
	// Set up address and data registers.
	EEP_ADDR_REG=((*u16_addr)+offset);
	// Interrupts MUST BE DISABLED!
	// Preparing for EEPROM erasing.
	EEP_PREP_WRITE;
	// Start EEPROM write.
	EEP_START_WRITE;
	// Interrupts can be enabled.
}
#endif	/*EEP_NO_ERASE*/

// Write a byte into EEPROM.
// This function turns off interrupts globally and turns them back on by itself!
// [u16_addr] - base EEPROM address;
// [offset] - offset from base address in bytes;
// [u8_data] - data byte to be written;
#ifdef EEP_16BIT_ADDR
void EEPROM_write_byte(const uint16_t *u16_addr, uint16_t offset, uint8_t u8_data)
#else
void EEPROM_write_byte(const uint16_t *u16_addr, uint8_t offset, uint8_t u8_data)
#endif	/*EEP_16BIT_ADDR*/
{
	// Wait for completion of previous write.
	WAIT_EEP_WR wdt_reset();
	WAIT_SELF_WR wdt_reset();
	// Set programming mode.
	EEP_SET_WR1; EEP_SET_WR2;
	// Set up address and data registers.
	EEP_ADDR_REG=((*u16_addr)+offset);
	EEP_DATA_REG=~u8_data;
	// Interrupts MUST BE DISABLED!
	cli();
	// Preparing for EEPROM writing (after erasing).
	EEP_PREP_WRITE;
	// Start EEPROM write.
	EEP_START_WRITE;
	// Interrupts can be enabled.
	sei();
}

// Write a byte into EEPROM.
// This function needs global interrupts to be turned off before call!
// [u16_addr] - base EEPROM address;
// [offset] - offset from base address in bytes;
// [u8_data] - data byte to be written;
#ifdef EEP_16BIT_ADDR
void EEPROM_write_byte_intfree(const uint16_t *u16_addr, uint16_t offset, uint8_t u8_data)
#else
void EEPROM_write_byte_intfree(const uint16_t *u16_addr, uint8_t offset, uint8_t u8_data)
#endif	/*EEP_16BIT_ADDR*/
{
	// Wait for completion of previous write.
	WAIT_EEP_WR wdt_reset();
	WAIT_SELF_WR wdt_reset();
	// Set programming mode.
	EEP_SET_WR1; EEP_SET_WR2;
	// Set up address and data registers.
	EEP_ADDR_REG=((*u16_addr)+offset);
	EEP_DATA_REG=~u8_data;
	// Interrupts MUST BE DISABLED!
	// Preparing for EEPROM writing (after erasing).
	EEP_PREP_WRITE;
	// Start EEPROM write.
	EEP_START_WRITE;
	// Interrupts can be enabled.
}

// Read a byte from EEPROM.
// This function is not interrupt-sensitive.
// [u16_addr] - base EEPROM address;
// [offset] - offset from base address in bytes;
// [u8_data] - pointer where data will be read to;
#ifdef EEP_16BIT_ADDR
void EEPROM_read_byte(const uint16_t *u16_addr, uint16_t offset, uint8_t *u8_data)
#else
void EEPROM_read_byte(const uint16_t *u16_addr, uint8_t offset, uint8_t *u8_data)
#endif	/*EEP_16BIT_ADDR*/
{
	// Wait for completion of previous write.
	WAIT_EEP_WR wdt_reset();
	WAIT_SELF_WR wdt_reset();
	// Set up address register.
	EEP_ADDR_REG=((*u16_addr)+offset);
	// Start EEPROM read by writing EERE.
	EEP_START_READ;
	// Return data from data register.
	(*u8_data)=~EEP_DATA_REG;
}

// Calculate CRC-8 for all [EEPROM_STORE_SIZE] bytes of data.
// Returns calculated CRC-8 byte.
uint8_t EEPROM_calc_CRC(void)
{
	uint8_t read_data, CRC_data;
#ifdef EEP_16BIT_ADDR
	uint16_t cycle;
#else
	uint8_t cycle;
#endif
	// Init CRC.
	CRC_data=CRC8_init();
	// Cycle through all data (except last byte).
	cycle=0;
	while(cycle<EEPROM_CRC_POSITION)
	{
		// Read byte from EEPROM.
		EEPROM_read_byte(&u16_current_address, cycle, &read_data);
		// Data read ok, calculate CRC for this byte.
		CRC_data=CRC8_calc(CRC_data, read_data);
		cycle++;
	}
	return CRC_data;
}

// Search a data segment in EEPROM and read it back. 
// Also this function sets up local [u16_current_address] that holds base address for all EEPROM operations via [EEPROM_read_segment()] and [EEPROM_write_segment()] funtions.
// This function is not interrupt-sensitive.
// [data] - pointer to an array where data will be read to if it will be found successfully;
// [start] - offset (in bytes) from start of the detected data in EEPROM from which data will be read into [data] starting from 0;
// [end] - offset (in bytes) from start of the detected data in EEPROM up to which data will be read into [data];
// Returns [EEPROM_NO_DATA] if no marked segment was found in EEPROM or data is corrupted.
// Returns [EEPROM_OK] if data segment was successfully found and read into [data].
#ifdef EEP_16BIT_ADDR
uint8_t EEPROM_search_data(uint8_t *data, uint16_t start, uint16_t end)
#else
uint8_t EEPROM_search_data(uint8_t *data, uint8_t start, uint8_t end)
#endif	/*EEP_16BIT_ADDR*/
{
	uint8_t u8_answer, u8_data_found;
#ifdef EEP_16BIT_ADDR
	uint16_t cycle, data_index;
#else
	uint8_t cycle, data_index;
#endif
	uint16_t u16_offset;
	// Reset to default address.
	u16_current_address=0x0;
	u16_offset=0;
	// Search marker through all segments of EEPROM.
	do
	{
		// Try to read first symbol (detect marker).
		EEPROM_read_byte(&u16_offset, 0, &u8_data_found);
		if(u8_data_found==EEPROM_START_MARKER)
		{
			// Save new address.
			u16_current_address=u16_offset;
			// Calculate CRC for the detected data.
			u8_answer=EEPROM_calc_CRC();
			// Read CRC byte from EEPROM.
			EEPROM_read_byte(&u16_current_address, EEPROM_CRC_POSITION, &u8_data_found);
			// Check CRC match.
			if(u8_data_found==u8_answer)
			{
				// Read settings.
				data_index=0;
				cycle=start;
				while(cycle<=end)
				{
					EEPROM_read_byte(&u16_current_address, cycle, data+data_index);
					data_index++;
					cycle++;
				}
				return EEPROM_OK;
			}
			// CRC mismatch.
		}
		// Go to the next segment.
		u16_offset+=EEPROM_STORE_SIZE;
	}
	while(u16_offset<=(EEPROM_ROM_SIZE-EEPROM_STORE_SIZE));
	// Run out of EEPROM and no valid data found.
	// Reset base address.
	u16_current_address=0;
	// [data] is not affected.
	return EEPROM_NO_DATA;
}

// Read a data segment from EEPROM within [EEPROM_STORE_SIZE] bytes at [u16_current_address] address.
// This function is not interrupt-sensitive.
// [data] - pointer where data will be read to;
// [offset] - offset from base address in EEPROM in bytes;
// [count] - how many bytes to read;
#ifdef EEP_16BIT_ADDR
void EEPROM_read_segment(uint8_t *data, uint16_t offset, uint16_t count)
#else
void EEPROM_read_segment(uint8_t *data, uint8_t offset, uint8_t count)
#endif	/*EEP_16BIT_ADDR*/
{
#ifdef EEP_16BIT_ADDR
	uint16_t cycle;
#else
	uint8_t cycle;
#endif
	if((offset+count)>EEPROM_STORE_SIZE) return;
	for(cycle=0;cycle<count;cycle++)
	{
		EEPROM_read_byte(&u16_current_address, offset+cycle, data+cycle);
	}
}

// Write a data segment into EEPROM within [EEPROM_STORE_SIZE] bytes at [u16_current_address] address.
// This function turns off interrupts globally and turns them back on by itself!
// [data] - pointer to the data that will be written into EEPROM;
// [offset] - offset from base address in EEPROM in bytes;
// [count] - how many bytes to write;
#ifdef EEP_16BIT_ADDR
void EEPROM_write_segment(uint8_t *data, uint16_t offset, uint16_t count)
#else
void EEPROM_write_segment(uint8_t *data, uint8_t offset, uint8_t count)
#endif	/*EEP_16BIT_ADDR*/
{
#ifdef EEP_16BIT_ADDR
	uint16_t cycle;
#else
	uint8_t cycle;
#endif	/*EEP_16BIT_ADDR*/
	uint8_t tmp_data;
	if((offset+count)>EEPROM_STORE_SIZE) return;
#ifndef EEP_NO_ERASE
	for(cycle=0;cycle<count;cycle++)
	{
		// Read current byte (trying do decide, do we need to update it at all).
		EEPROM_read_byte(&u16_current_address, offset+cycle, &tmp_data);
		if(tmp_data!=data[cycle])
		{
			// Byte has to be changed.
			if(data[cycle]==EEPROM_ERASED_DATA)
			{
				// No need to write data, only erase the byte.
				EEPROM_erase_byte(&u16_current_address, offset+cycle);
			}
			else
			{
				// Erase the byte.
				EEPROM_erase_byte(&u16_current_address, offset+cycle);
				// Write the new value.
				EEPROM_write_byte(&u16_current_address, offset+cycle, data[cycle]);
			}
		}		
	}
#else
	for(cycle=0;cycle<count;cycle++)
	{
		// Read current byte (trying do decide, do we need to update it at all).
		EEPROM_read_byte(&u16_current_address, offset+cycle, &tmp_data);
		if(tmp_data!=data[cycle])
		{
			// Byte has to be changed.
			// Write the new value.
			EEPROM_write_byte(&u16_current_address, offset+cycle, data[cycle]);
		}
	}
#endif	/*EEP_NO_ERASE*/
	// Re-calculate CRC of all stored data.
	cycle=EEPROM_calc_CRC();
#ifndef EEP_NO_ERASE
	// Erase CRC.
	EEPROM_erase_byte(&u16_current_address, EEPROM_CRC_POSITION);
#endif	/*EEP_NO_ERASE*/
	// Rewrite CRC.
	EEPROM_write_byte(&u16_current_address, EEPROM_CRC_POSITION, cycle);
}

// Write a data segment into EEPROM within [EEPROM_STORE_SIZE] bytes at [u16_current_address] address.
// This function needs global interrupts to be turned off before call!
// [data] - pointer to the data that will be written into EEPROM;
// [offset] - offset from base address in EEPROM in bytes;
// [count] - how many bytes to write;
#ifdef EEP_16BIT_ADDR
void EEPROM_write_segment_intfree(uint8_t *data, uint16_t offset, uint16_t count)
#else
void EEPROM_write_segment_intfree(uint8_t *data, uint8_t offset, uint8_t count)
#endif	/*EEP_16BIT_ADDR*/
{
#ifdef EEP_16BIT_ADDR
	uint16_t cycle;
#else
	uint8_t cycle;
#endif	/*EEP_16BIT_ADDR*/
	uint8_t tmp_data;
	if((offset+count)>EEPROM_STORE_SIZE) return;
#ifndef EEP_NO_ERASE
	for(cycle=0;cycle<count;cycle++)
	{
		// Read current byte (trying do decide, do we need to update it at all).
		EEPROM_read_byte(&u16_current_address, offset+cycle, &tmp_data);
		if(tmp_data!=data[cycle])
		{
			// Byte has to be changed.
			if(data[cycle]==EEPROM_ERASED_DATA)
			{
				// No need to write data, only erase the byte.
				EEPROM_erase_byte_intfree(&u16_current_address, offset+cycle);
			}
			else
			{
				// Erase the byte.
				EEPROM_erase_byte_intfree(&u16_current_address, offset+cycle);
				// Write the new value.
				EEPROM_write_byte_intfree(&u16_current_address, offset+cycle, data[cycle]);
			}
		}
	}
#else
	for(cycle=0;cycle<count;cycle++)
	{
		// Read current byte (trying do decide, do we need to update it at all).
		EEPROM_read_byte(&u16_current_address, offset+cycle, &tmp_data);
		if(tmp_data!=data[cycle])
		{
			// Byte has to be changed.
			// Write the new value.
			EEPROM_write_byte_intfree(&u16_current_address, offset+cycle, data[cycle]);
		}
	}
#endif	/*EEP_NO_ERASE*/
	// Re-calculate CRC of all stored data.
	cycle=EEPROM_calc_CRC();
#ifndef EEP_NO_ERASE
	// Erase CRC.
	EEPROM_erase_byte_intfree(&u16_current_address, EEPROM_CRC_POSITION);
#endif	/*EEP_NO_ERASE*/
	// Rewrite CRC.
	EEPROM_write_byte_intfree(&u16_current_address, EEPROM_CRC_POSITION, cycle);
}

// Move all data to next segment (wear leveling).
// Procedure is fail-safe: data retains even on power loss condition at all times.
// This function turns off interrupts globally and turns them back on by itself!
void EEPROM_goto_next_segment(void)
{
	uint16_t temp_addr;
#ifdef EEP_16BIT_ADDR
	uint16_t cycle;
#else
	uint8_t cycle;
#endif
	uint8_t read_data, tmp_data;
	// Check current EEPROM position.
	temp_addr=EEPROM_ROM_SIZE-u16_current_address;
	if(temp_addr<=EEPROM_STORE_SIZE)
	{
		// No place for next segment before end of EEPROM space.
		// Reset address.
		temp_addr=0;
	}
	else
	{
		// Calculate new address.
		temp_addr=u16_current_address+EEPROM_STORE_SIZE;
	}
	// Preventing data corruption: first, copy data to the next segment,
	// then erase the current segment.
	// If first step will be interrupted, not changed data will be found in current segment after restart;
	// if second step will be interrupted, corrupted data in current segment will be skipped (with help of CRC),
	// and not corrupted data will be found in next segment.
	
	// First step: copy all data to the next address.
	// Cycle through all data.
	for(cycle=0;cycle<EEPROM_STORE_SIZE;cycle++)
	{
		cli();
		// Read data from EEPROM at old address.
		EEPROM_read_byte(&u16_current_address, cycle, &read_data);
#ifndef EEP_NO_ERASE
		// Read target byte (trying do decide, do we need to rewrite it at all).
		EEPROM_read_byte(&temp_addr, cycle, &tmp_data);
		if(tmp_data!=read_data)
		{
			// Erase the byte only if it is not already empty.
			if(tmp_data!=EEPROM_ERASED_DATA)
			{
				// Prepare (erase) new location in EEPROM.
				EEPROM_erase_byte_intfree(&temp_addr, cycle);
			}
			// Rewrite the byte only if it is not needed to be 0xFF;
			if(read_data!=EEPROM_ERASED_DATA)
			{
				// Write data into EEPROM at new location.
				EEPROM_write_byte_intfree(&temp_addr, cycle, read_data);
			}
		}
#else
		// Read target byte (trying do decide, do we need to rewrite it at all).
		EEPROM_read_byte(&temp_addr, cycle, &tmp_data);
		if(tmp_data!=read_data)
		{
			// Write data into EEPROM at new location.
			EEPROM_write_byte_intfree(&temp_addr, cycle, read_data);
		}
#endif	/*EEP_NO_ERASE*/
		sei();
	}
	// Second step: erase all data at the old address.
	// Cycle through all old data.
	for(cycle=0;cycle<EEPROM_STORE_SIZE;cycle++)
	{
		cli();
		// Read a byte (trying do decide, do we need to erase it at all).
		EEPROM_read_byte(&u16_current_address, cycle, &tmp_data);
		if(tmp_data!=EEPROM_ERASED_DATA)
		{
#ifndef EEP_NO_ERASE
			// Erase data at old location.
			EEPROM_erase_byte_intfree(&u16_current_address, cycle);
#else
			// Rewrite data at old location with "0xFF".
			EEPROM_write_byte_intfree(&u16_current_address, cycle, EEPROM_ERASED_DATA);
#endif	/*EEP_NO_ERASE*/
		}
		sei();
	}
	// Set new internal data address.
	u16_current_address=temp_addr;
}
