/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

#include "mem.h"
#include "interp.h"
#include "persist.h"

// #if defined(LVGL)
// #include "lv_conf.h"
// #include <lvgl.h>

// extern "C" {
// 	void update_lvgl(void) {
// 	 lv_tick_inc(1);
//      lv_timer_handler();
 
//    }

// }


// #endif

#if defined(ESP32_ORIGINAL)
#include <LittleFS.h>
#endif

void setup() {
	
#if defined(ESP32_ORIGINAL)
if (!LittleFS.begin()) {
         if (!LittleFS.format()) {
        } else
        !LittleFS.begin();
        }
	

#endif
#ifdef ARDUINO_NRF52_PRIMO
	sd_softdevice_disable();
#endif
	memInit();
	primsInit();
	hardwareInit();
	outputString((char *) "Welcome to MicroBlocks!");
	restoreScripts();
	if (BLE_isEnabled()) BLE_start();
	startAll();
}

void loop() {
	vmLoop();
}
