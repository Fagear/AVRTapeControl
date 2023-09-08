/*
 * strings.c
 *
 * Created: 07.09.2023 17:29:20
 *  Author: kryukov
 */ 

#include "strings.h"

const uint8_t cch_startup_1[] PROGMEM = "\n\r\n\rFirmware OK\n\r";
const uint8_t cch_endl[] PROGMEM = "\n\r";
const uint8_t cch_arrow[] PROGMEM = "->";
const uint8_t cch_mode_powerup[] PROGMEM = "POWER_UP";
const uint8_t cch_mode_to_init[] PROGMEM = "TO_INIT";
const uint8_t cch_mode_init[] PROGMEM = "INIT";
const uint8_t cch_mode_to_stop[] PROGMEM = "TO_STOP";
const uint8_t cch_mode_stop[] PROGMEM = "STOP";
const uint8_t cch_mode_wait_stop[] PROGMEM = "WAIT_STOP";
const uint8_t cch_mode_to_start[] PROGMEM = "TO_START";
const uint8_t cch_mode_wait_dir[] PROGMEM = "WAIT_DIR";
const uint8_t cch_mode_hd_dir_sel[] PROGMEM = "HEAD_DIR_SEL";
const uint8_t cch_mode_wait_pinch[] PROGMEM = "WAIT_PINCH";
const uint8_t cch_mode_pinch_sel[] PROGMEM = "PINCH_SEL";
const uint8_t cch_mode_wait_takeup[] PROGMEM = "WAIT_TAKEUP";
const uint8_t cch_mode_tu_dir_sel[] PROGMEM = "TAKEUP_DIR_SEL";
const uint8_t cch_mode_wait_run[] PROGMEM = "WAIT_RUN";
const uint8_t cch_mode_pb_fwd[] PROGMEM = "PB_FWD";
const uint8_t cch_mode_pb_rev[] PROGMEM = "PB_REV";
const uint8_t cch_mode_fw_fwd[] PROGMEM = "FW_FWD";
const uint8_t cch_mode_fw_rev[] PROGMEM = "FW_REV";
const uint8_t cch_mode_fw_fwd_hd_rev[] PROGMEM = "FW_FWD_HD_REV";
const uint8_t cch_mode_fw_rev_hd_rev[] PROGMEM = "FW_REV_HD_REV";
const uint8_t cch_mode_to_halt[] PROGMEM = "TO_HALT";
const uint8_t cch_mode_halt[] PROGMEM = "HALT";
const uint8_t cch_mode_unknown[] PROGMEM = "UNKNOWN";
const uint8_t cch_enabled[] PROGMEM = "ENABLED\n\r";
const uint8_t cch_disabled[] PROGMEM = "DISABLED\n\r";
const uint8_t cch_set_auto_rewind[] PROGMEM = "Auto-rewind: ";
const uint8_t cch_set_reverse[] PROGMEM = "Reverse operations: ";
const uint8_t cch_set_auto_reverse_ab[] PROGMEM = "Auto-reverse (A-B-stop): ";
const uint8_t cch_set_auto_reverse_loop[] PROGMEM = "Auto-reverse (loop): ";
const uint8_t cch_startup_delay[] PROGMEM = "Performing start-up delay...\n\r";
const uint8_t cch_unknown_mode[] PROGMEM = "Unknown new mode, switched to STOP\n\r";
const uint8_t cch_stop_active[] PROGMEM = "Logic: STOP, TTR: ACTIVE, forcing into STOP\n\r";
const uint8_t cch_ttr_halt[] PROGMEM = "Tape transport halted!";
const uint8_t cch_bad_drive1[] PROGMEM = "Logic: STOP, TTR: bad tacho\n\r";
const uint8_t cch_bad_drive2[] PROGMEM = " Bad motor or belts.\n\r";
const uint8_t cch_no_tape[] PROGMEM = "No tape, stopping...\n\r";
const uint8_t cch_no_tacho_pb[] PROGMEM = "No PB tacho";
const uint8_t cch_no_tacho_fw[] PROGMEM = "No FW tacho";
const uint8_t cch_auto_reverse[] PROGMEM = ", auto-reverse ";
const uint8_t cch_reverse_fwd_rev[] PROGMEM = "FWD->REV queued\n\r";
const uint8_t cch_reverse_rev_fwd[] PROGMEM = "REV->FWD queued\n\r";
const uint8_t cch_auto_stop[] PROGMEM = ", auto-stop";
const uint8_t cch_tape_end[] PROGMEM = " at the end\n\r";
const uint8_t cch_auto_rewind[] PROGMEM = ", auto-rewind queued\n\r";
const uint8_t cch_stop_corr[] PROGMEM = "TTR went STOP, correcting logic into STOP\n\r";
const uint8_t cch_halt_stop1[] PROGMEM = "Logic: ACTIVE, TTR: STOP\n\r";
const uint8_t cch_halt_stop2[] PROGMEM = " Bad solenoid or bad capstan drive or low voltage.\n\r";
const uint8_t cch_halt_force_stop[] PROGMEM = "Logic: HALT, TTR: active, forcing into STOP\n\r";
const uint8_t cch_new_user_mode[] PROGMEM = "User mode changed: ";
