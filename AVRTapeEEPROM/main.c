#include <conio.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// EEPROM settings offsets (NOTE: keep in sync with enum in [avrtape.h]).
enum
{
    EPS_MARKER,						// Start marker position
    EPS_TTR_TYPE,					// Transport type (if several types are enabled on compile time)
    EPS_TTR_FTRS,					// Transport features (tacho in stop, reverse enable, etc.)
    EPS_SRV_FTRS,					// Service features (auto-reverse, auto-rewind, etc.)
};

// EEPROM settings (NOTE: keep in sync with defines in [config.h] and [drv_eeprom.h]).
#define EEPROM_START_MARKER		0xA5
#define SETTINGS_SIZE		5		// Number of bytes for full [settings_data] union
#define EEPROM_STORE_SIZE	16		// EEPROM settings length
#define EEPROM_CRC_POSITION	(EEPROM_STORE_SIZE-1)

// Supported tape transports (NOTE: keep in sync with enum in [avrtape.h]).
enum
{
    TTR_TYPE_TANASHIN,				// Tanashin TN-21ZLG clone mechanism from AliExpress
    TTR_TYPE_CRP42602Y,				// CRP42602Y mechanism from AliExpress
    TTR_TYPE_KENWOOD,				// Kenwood mechanism
    TTR_TYPE_COUNT
};

static const uint8_t lut_ttr_names[TTR_TYPE_COUNT][64] =
{
    "Tanashin TN-21ZLG/CSG/clone mechanism (M60207052)",
    "CRP42602Y mechanism (M02753900D)",
    "Kenwood mechanism (not supported)",
};

// Flags for transport features for (NOTE: keep in sync with enum in [common_log.h]).
enum
{
    TTR_FEA_STOP_TACHO	= (1<<0),	// Enable checking tachometer in STOP mode (some CRP42602Y can do that)
    TTR_FEA_REV_ENABLE	= (1<<1),	// Enable all operations with reverse playback (does affect [SRV_FEA_PB_AUTOREV] and [SRV_FEA_PB_LOOP])
};

// Flags for service features for (NOTE: keep in sync with enum in [common_log.h]).
enum
{
    SRV_FEA_TWO_PLAYS	= (1<<0),	// Enable two PLAY buttons/LEDs (for each direction)
    SRV_FEA_ONE2REC		= (1<<1),	// Enable one-button record start (no need to press any PLAY button)
    SRV_FEA_PB_AUTOREV	= (1<<2),	// Enable auto-reverse for forward playback (PB FWD -> PB REV -> STOP)
    SRV_FEA_PB_LOOP		= (1<<3),	// Enable full auto-reverse (PB FWD -> PB REV -> PB FWD -> ...)
    SRV_FEA_PBF2REW		= (1<<4),	// Enable auto-rewind for forward PLAY (PB FWD -> FW REV -> STOP) (lower priority than [SRV_FEA_PB_LOOP])
    SRV_FEA_FF2REW		= (1<<5),	// Enable auto-rewind for fast forward (FW FWD -> FW REV -> STOP)
};

#define TTR_FEA_DEFAULT				(TTR_FEA_REV_ENABLE)	// Default transport feature settings
#define SRV_FEA_DEFAULT				(SRV_FEA_TWO_PLAYS|SRV_FEA_ONE2REC|SRV_FEA_PB_AUTOREV/*|SRV_FEA_PB_LOOP*SRV_FEA_PBF2REW|SRV_FEA_FF2REW*/)		// Default service feature settings

// NOTE: keep in sync with array in [calc_crc.c].
static const uint8_t lut_crc8[256] = {
    0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97,
    0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
    0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4,
    0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
    0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11,
    0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
    0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52,
    0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
    0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA,
    0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
    0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9,
    0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
    0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C,
    0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
    0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F,
    0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
    0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED,
    0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
    0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE,
    0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
    0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B,
    0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
    0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28,
    0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
    0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0,
    0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
    0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93,
    0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
    0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56,
    0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
    0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15,
    0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC
};

volatile const uint8_t ucaf_compile_time[] = __TIME__;		// Time of compilation
volatile const uint8_t ucaf_compile_date[] = __DATE__;		// Date of compilation
volatile const uint8_t ucaf_info[] = "ATmega tape controller EEPROM image creator";		// Software description
volatile const uint8_t ucaf_author[] = "Maksim Kryukov aka Fagear";					// Author
volatile const uint8_t ucaf_url[] = "https://github.com/Fagear/AVRTapeControl";		// URL
volatile const uint8_t ucaf_eep[] = "avrtape_eep.bin";     // EEPROM file name

uint8_t CRC8_init(void)
{
    return 0xFF;
}

uint8_t CRC8_calc(uint8_t CRC_data, uint8_t in_data)
{
    uint8_t offset;
    offset=CRC_data^in_data;
    return lut_crc8[offset];
}

uint8_t get_bin_selector(void)
{
    uint8_t in_select;
    // Input ASCII char.
    in_select = (uint8_t)getch();
    // Convert ASCII code to a digit.
    in_select -= '0';
    if((in_select != 0)&&(in_select != 1))
    {
        printf("Wrong input: %u. Corrected to 0.\n\r", in_select);
        in_select = 0;
    }
    else
    {
        printf("Selected: %u.\n\r", in_select);
    }
    return in_select;
}

void print_settings(uint8_t *set_arr)
{
    uint8_t selector;
    printf("\n\rSettings to be written into EEPROM:\n\r");

    printf("Tape transport: %s\n\r", lut_ttr_names[set_arr[EPS_TTR_TYPE]]);

    selector = (set_arr[EPS_SRV_FTRS] & SRV_FEA_ONE2REC);
    printf("Record engagement method: %s\n\r", selector ? "only [REC]" : "[PLAY] holding [REC]");

    selector = (set_arr[EPS_TTR_FTRS] & TTR_FEA_REV_ENABLE);
    printf("Reverse function: %s\n\r", selector ? "yes" : "no");

    selector = (set_arr[EPS_SRV_FTRS] & SRV_FEA_PB_AUTOREV);
    printf("Playback/record auto-reverse (A->B): %s\n\r", selector ? "yes" : "only auto-stop");

    selector = (set_arr[EPS_SRV_FTRS] & SRV_FEA_PB_LOOP);
    printf("Playback loop (A->B->A->...): %s\n\r", selector ? "yes" : "no");

    selector = (set_arr[EPS_SRV_FTRS] & SRV_FEA_PBF2REW);
    printf("Auto-rewind after forward playback/record: %s\n\r", selector ? "yes" : "no");

    selector = (set_arr[EPS_SRV_FTRS] & SRV_FEA_FF2REW);
    printf("Auto-rewind after fast forward: %s\n\r", selector ? "yes" : "no");
}

void eep_conditioning(uint8_t *set_arr)
{
    uint8_t crc_data, idx;

    // Calculate CRC for settings data.
    crc_data = CRC8_init();
    idx = 0;
    while(idx<EEPROM_CRC_POSITION)
    {
        crc_data = CRC8_calc(crc_data, set_arr[idx]);
        idx++;
    }
    set_arr[EEPROM_CRC_POSITION] = crc_data;

    // Invert data to reduce EEPROM wear.
    idx = 0;
    while(idx<EEPROM_STORE_SIZE)
    {
        set_arr[idx] = ~set_arr[idx];
        idx++;
    }
}

int main()
{
    uint8_t u8a_settings[EEPROM_STORE_SIZE];
    uint8_t in_select;

    printf("%s\n\r", ucaf_info);
    printf("%s\n\r", ucaf_author);
    printf("%s, %s\n\r", ucaf_compile_date, ucaf_compile_time);
    printf("%s\n\r", ucaf_url);

    // Zero out EEPROM image.
    for(uint16_t idx = 0; idx < EEPROM_STORE_SIZE; idx++)
    {
        u8a_settings[idx] = 0;
    }

    // Put in starting marker and default settings.
    u8a_settings[EPS_MARKER] = EEPROM_START_MARKER;
    u8a_settings[EPS_TTR_FTRS] = TTR_FEA_DEFAULT;
    u8a_settings[EPS_SRV_FTRS] = SRV_FEA_DEFAULT;

    printf("\n\rSelect tape transport mech:\n\r");
    printf("1 - %s\n\r", lut_ttr_names[TTR_TYPE_TANASHIN]);
    printf("2 - %s\n\r", lut_ttr_names[TTR_TYPE_CRP42602Y]);
    printf("3 - %s\n\r", lut_ttr_names[TTR_TYPE_KENWOOD]);
    u8a_settings[EPS_TTR_TYPE] = (uint8_t)getch();
    u8a_settings[EPS_TTR_TYPE] -= '1';
    if((u8a_settings[EPS_TTR_TYPE]!=TTR_TYPE_TANASHIN)&&
        (u8a_settings[EPS_TTR_TYPE]!=TTR_TYPE_CRP42602Y)&&
        (u8a_settings[EPS_TTR_TYPE]!=TTR_TYPE_KENWOOD))
    {
        printf("Wrong input: %u\n\r", (uint8_t)(u8a_settings[EPS_TTR_TYPE] + 1));
        return -1;
    }
    else
    {
        printf("Selected: %u.\n\r", (u8a_settings[EPS_TTR_TYPE] + 1));
    }

    printf("\n\rSelect recording engagement method:\n\r");
    printf("0 - [PLAY] while holding [RECORD]\n\r");
    printf("1 - [RECORD] (one button)\n\r");
    in_select = get_bin_selector();
    if(in_select == 0)
    {
        u8a_settings[EPS_SRV_FTRS] &= ~(SRV_FEA_ONE2REC);
    }
    else
    {
        u8a_settings[EPS_SRV_FTRS] |= (SRV_FEA_ONE2REC);
    }

    if(u8a_settings[EPS_TTR_TYPE]==TTR_TYPE_TANASHIN)
    {
        // Disable functions for non-reverse mech.
        u8a_settings[EPS_TTR_FTRS] &= ~(TTR_FEA_STOP_TACHO|TTR_FEA_REV_ENABLE);
        u8a_settings[EPS_SRV_FTRS] &= ~(SRV_FEA_TWO_PLAYS|SRV_FEA_PB_AUTOREV|SRV_FEA_PB_LOOP);
    }
    else
    {
        // Enable functions of reverse mech.
        u8a_settings[EPS_TTR_FTRS] |= (TTR_FEA_REV_ENABLE);

        printf("\n\rAutoreverse playback/record (A->B->stop):\n\r");
        printf("0 - disable\n\r");
        printf("1 - enable\n\r");
        in_select = get_bin_selector();
        if(in_select == 0)
        {
            u8a_settings[EPS_SRV_FTRS] &= ~(SRV_FEA_PB_AUTOREV|SRV_FEA_PB_LOOP);
        }
        else
        {
            u8a_settings[EPS_SRV_FTRS] |= (SRV_FEA_PB_AUTOREV);
        }

        if((u8a_settings[EPS_SRV_FTRS] & SRV_FEA_PB_AUTOREV) != 0)
        {
            printf("\n\rPlayback reverse loop (A->B->A->B->...):\n\r");
            printf("0 - disable\n\r");
            printf("1 - enable\n\r");
            in_select = get_bin_selector();
            if(in_select == 0)
            {
                u8a_settings[EPS_SRV_FTRS] &= ~(SRV_FEA_PB_LOOP);
            }
            else
            {
                u8a_settings[EPS_SRV_FTRS] |= (SRV_FEA_PB_LOOP);
            }
        }
    }

    if((u8a_settings[EPS_SRV_FTRS] & SRV_FEA_PB_AUTOREV) == 0)
    {
        printf("\n\rAuto-rewind after playback/record forward:\n\r");
        printf("0 - disable\n\r");
        printf("1 - enable\n\r");
        in_select = get_bin_selector();
        if(in_select == 0)
        {
            u8a_settings[EPS_SRV_FTRS] &= ~(SRV_FEA_PBF2REW);
        }
        else
        {
            u8a_settings[EPS_SRV_FTRS] |= (SRV_FEA_PBF2REW);
        }
    }

    printf("\n\rAuto-rewind after fast forward (tape retentioning):\n\r");
    printf("0 - disable\n\r");
    printf("1 - enable\n\r");
    in_select = get_bin_selector();
    if(in_select == 0)
    {
        u8a_settings[EPS_SRV_FTRS] &= ~(SRV_FEA_PBF2REW);
    }
    else
    {
        u8a_settings[EPS_SRV_FTRS] |= (SRV_FEA_FF2REW);
    }

    // Printout settings.
    print_settings(u8a_settings);

    // Condition data to be written to EEPROM.
    eep_conditioning(u8a_settings);

    FILE *eep_dump = fopen((const char *)ucaf_eep, "wb");
    if(eep_dump)
    {
        fwrite(u8a_settings, 1, EEPROM_STORE_SIZE, eep_dump);
        fclose(eep_dump);
        printf("\n\rSaved EEPROM image to a file %s\n\r", ucaf_eep);
        return 0;
    }
    else
    {
        printf("\n\rFailed to write EEPROM image!\n\r");
        return -2;
    }
}
