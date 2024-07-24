/**************************************************************************************************************************************************************
strings.h

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

Created: 2023-09-07

Strings for UART output to put into ROM of AVR MCUs.

**************************************************************************************************************************************************************/

#ifndef STRINGS_H_
#define STRINGS_H_

#include <stdio.h>
#include <avr/pgmspace.h>
#include "config.h"		// Contains [UART_TERM]

#ifdef UART_TERM

extern const uint8_t cch_startup_1[];
extern const uint8_t cch_endl[];
extern const uint8_t cch_arrow[];
extern const uint8_t cch_neq[];
extern const uint8_t cch_mode_powerup[];
extern const uint8_t cch_mode_to_init[];
extern const uint8_t cch_mode_init[];
extern const uint8_t cch_mode_to_stop[];
extern const uint8_t cch_mode_stop[];
extern const uint8_t cch_mode_wait_stop[];
extern const uint8_t cch_mode_to_start[];
extern const uint8_t cch_mode_wait_dir[];
extern const uint8_t cch_mode_hd_dir_sel[];
extern const uint8_t cch_mode_wait_pinch[];
extern const uint8_t cch_mode_pinch_sel[];
extern const uint8_t cch_mode_wait_takeup[];
extern const uint8_t cch_mode_tu_dir_sel[];
extern const uint8_t cch_mode_wait_run[];
extern const uint8_t cch_mode_pb_fwd[];
extern const uint8_t cch_mode_pb_rev[];
extern const uint8_t cch_mode_rc_fwd[];
extern const uint8_t cch_mode_rc_rev[];
extern const uint8_t cch_mode_fw_fwd[];
extern const uint8_t cch_mode_fw_rev[];
extern const uint8_t cch_mode_fw_fwd_hd_rev[];
extern const uint8_t cch_mode_fw_rev_hd_rev[];
extern const uint8_t cch_mode_to_halt[];
extern const uint8_t cch_mode_halt[];
extern const uint8_t cch_mode_unknown[];
extern const uint8_t cch_tape_transport[];
extern const uint8_t cch_enabled[];
extern const uint8_t cch_disabled[];
extern const uint8_t cch_one[];
extern const uint8_t cch_two[];
extern const uint8_t cch_forward[];
extern const uint8_t cch_reverse[];
extern const uint8_t cch_set_reverse[];
extern const uint8_t cch_set_auto_reverse_ab[];
extern const uint8_t cch_set_auto_reverse_loop[];
extern const uint8_t cch_set_pb_auto_rewind[];
extern const uint8_t cch_set_fw_auto_rewind[];
extern const uint8_t cch_set_tacho_stop[];
extern const uint8_t cch_set_pb_btns[];
extern const uint8_t cch_startup_delay[];
extern const uint8_t cch_pb_dir[];
extern const uint8_t cch_stop_active[];
extern const uint8_t cch_active_stop[];
extern const uint8_t cch_halt_active[];
extern const uint8_t cch_stop_tacho[];
extern const uint8_t cch_unknown_mode[];
extern const uint8_t cch_stop_corr[];
extern const uint8_t cch_force_stop[];
extern const uint8_t cch_mode_failed[];
extern const uint8_t cch_ttr_halt[];
extern const uint8_t cch_halt_stop1[];
extern const uint8_t cch_halt_stop2[];
extern const uint8_t cch_halt_stop3[];
extern const uint8_t cch_no_tape[];
extern const uint8_t cch_no_tacho_pb[];
extern const uint8_t cch_no_tacho_fw[];
extern const uint8_t cch_auto_reverse[];
extern const uint8_t cch_reverse_fwd_rev[];
extern const uint8_t cch_reverse_rev_fwd[];
extern const uint8_t cch_auto_stop[];
extern const uint8_t cch_tape_end[];
extern const uint8_t cch_auto_rewind[];
extern const uint8_t cch_new_user_mode[];
extern const uint8_t cch_target2current1[];
extern const uint8_t cch_target2current2[];
extern const uint8_t cch_user2target1[];
extern const uint8_t cch_user2target2[];
extern const uint8_t cch_mode_done[];
extern const uint8_t cch_no_record[];
extern const uint8_t cch_capst_stop[];
extern const uint8_t cch_capst_start[];
extern const uint8_t cch_sleep_in[];
extern const uint8_t cch_sleep_out[];

#endif /* UART_TERM */

#endif /* STRINGS_H_ */