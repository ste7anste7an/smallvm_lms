/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// tftPrims.cpp - Microblocks TFT screen primitives and touch screen input
// Bernat Romagosa, November 2018



#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <stdio.h>
#include <stdlib.h>

#include "mem.h"
#include "interp.h"

#if defined(ESP32_ORIGINAL) || defined(C3LVGL) || defined(S3_CTF)
#include <LittleFS.h>
#include <FS.h>
#endif

int useTFT = false;
int isOLED1106 = false;

static int touchEnabled = false;
static int deferUpdates = false;

// Redefine this macro for displays that must explicitly push offscreen changes to the display
#define UPDATE_DISPLAY() { taskSleep(-1); } // yield after potentially slow TFT operations

#if defined(ARDUINO_CITILAB_ED1) || defined(ARDUINO_M5Stack_Core_ESP32) || \
	defined(ARDUINO_M5Stick_C) || defined(ARDUINO_ESP8266_WEMOS_D1MINI) || \
	defined(ARDUINO_NRF52840_CLUE) || defined(ARDUINO_IOT_BUS) || defined(SCOUT_MAKES_AZUL) || \
	defined(TTGO_RP2040) || defined(TTGO_DISPLAY) || defined(ARDUINO_M5STACK_Core2) || \
	defined(GAMEPAD_DISPLAY) || defined(PICO_ED) || defined(OLED_128_64) || defined(COCUBE) || \
	defined(ARDUINO_M5Atom_S3) || defined(LMSDISPLAY) || defined(LMS7789) || defined(UNIHIKER) ||\
	defined(M5Atom_S3_TFT) || defined(CYDC) || defined(CYDR) || defined(CYDS343) || defined(C3LVGL) || defined(ELECROW) ||\
	defined(S3_CTF)

	//sodb
	//#if !defined(TFT_ESPI)
		#define BLACK 0
		#define WHITE 65535
	//#endif

	#if defined(ARDUINO_CITILAB_ED1)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7735.h"

		#define TFT_CS	5
		#define TFT_DC	9
		#define TFT_RST	10
		#define TFT_WIDTH 128
		#define TFT_HEIGHT 128
		Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

		void tftInit() {
			tft.initR(INITR_144GREENTAB);
			tft.setRotation(0);
			tftClear();
			useTFT = true;
		}

	#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7735.h"

		#define TFT_CS	D4
		#define TFT_DC	D3
		#define TFT_RST	-1
		#define TFT_WIDTH 128
		#define TFT_HEIGHT 128
		Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

		void tftInit() {
			tft.initR(INITR_144GREENTAB);
			tft.setRotation(1);
			tftClear();
			useTFT = true;
		}

	#elif defined(ARDUINO_M5Stack_Core_ESP32)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ILI9341.h"
		#define TFT_CS	14
		#define TFT_DC	27
		#define TFT_RST	33
		#define TFT_WIDTH 320
		#define TFT_HEIGHT 240
		Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
		void tftInit() {
			// test TFT_RST to see if we need to invert the display
			// (from https://github.com/m5stack/M5Stack/blob/master/src/utility/In_eSPI.cpp)
			pinMode(TFT_RST, INPUT_PULLDOWN);
			delay(1);
			bool invertFlag = digitalRead(TFT_RST);
			pinMode(TFT_RST, OUTPUT);

			tft.begin(40000000); // Run SPI at 80MHz/2
			tft.setRotation(1);
			tft.invertDisplay(invertFlag);

			uint8_t m = 0x08 | 0x04; // RGB pixel order, refresh LCD right to left
			tft.sendCommand(ILI9341_MADCTL, &m, 1);
			tftClear();
			// Turn on backlight:
			pinMode(32, OUTPUT);
			digitalWrite(32, HIGH);
			useTFT = true;
		}

	#elif defined(ARDUINO_M5Stick_C)
		// Preliminary: this is not yet working...
		#include "Adafruit_GFX.h"

		#define TFT_CS		5
		#define TFT_DC		23
		#define TFT_RST		18

		#ifdef ARDUINO_M5Stick_Plus
			#include "Adafruit_ST7789.h"
			#define TFT_WIDTH	240
			#define TFT_HEIGHT	135
		#else
			#include "Adafruit_ST7735.h"
			#define TFT_WIDTH	160
			#define TFT_HEIGHT	80
		#endif

		#ifdef ARDUINO_M5Stick_Plus
			Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
		#else
			// make a subclass so we can adjust the x/y offsets
			class M5StickLCD : public Adafruit_ST7735 {
			public:
				M5StickLCD(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst) {}
				void setOffsets(int colOffset, int rowOffset) {
					_xstart = _colstart = colOffset;
					_ystart = _rowstart = rowOffset;
				}
			};
			M5StickLCD tft = M5StickLCD(TFT_CS, TFT_DC, TFT_RST);
		#endif

		int readAXP(int reg) {
			Wire1.beginTransmission(0x34);
			Wire1.write(reg);
			Wire1.endTransmission();
			Wire1.requestFrom(0x34, 1);
			return Wire1.available() ? Wire1.read() : 0;
		}

		void writeAXP(int reg, int value) {
			Wire1.beginTransmission(0x34);
			Wire1.write(reg);
			Wire1.write(value);
			Wire1.endTransmission();
		}

		void tftInit() {
			#ifdef ARDUINO_M5Stick_Plus
				tft.init(TFT_HEIGHT, TFT_WIDTH);
				tft.setRotation(3);
			#else
				tft.initR(INITR_MINI160x80);
				tft.setOffsets(26, 1);
				tft.setRotation(1);
			#endif
			tft.invertDisplay(true); // display must be inverted to give correct colors...
			tftClear();

			Wire1.begin(21, 22);
			Wire1.setClock(400000);

			// turn on LCD power pins (LD02 and LD03) = 0x0C
			// and for C+, turn on Ext (0x40) for the buzzer and DCDC1 (0x01) since M5Stack's init code does that
			int n = readAXP(0x12);
			writeAXP(0x12, n | 0x4D);

			int brightness = 12; // useful range: 7-12 (12 is max)
			n = readAXP(0x28);
			writeAXP(0x28, (brightness << 4) | (n & 0x0f)); // set brightness

			useTFT = true;
		}

	#elif defined(ARDUINO_M5STACK_Core2) && !defined(TFT_ESPI)
		// Preliminary: this is not yet working...
		#include "Adafruit_GFX.h"
		#include "Adafruit_ILI9341.h"
		#define TFT_CS	5
		#define TFT_DC	15
		#define TFT_WIDTH 320
		#define TFT_HEIGHT 240
		Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

		int readAXP(int reg) {
			Wire1.beginTransmission(0x34);
			Wire1.write(reg);
			Wire1.endTransmission();
			Wire1.requestFrom(0x34, 1);
			return Wire1.available() ? Wire1.read() : 0;
		}

		void writeAXP(int reg, int value) {
			Wire1.beginTransmission(0x34);
			Wire1.write(reg);
			Wire1.write(value);
			Wire1.endTransmission();
		}

		void AXP192_SetDCVoltage(uint8_t number, uint16_t voltage) {
			uint8_t addr;
			if (number > 2) return;
			voltage = (voltage < 700) ? 0 : (voltage - 700) / 25;
			switch (number) {
			case 0:
				addr = 0x26;
				break;
			case 1:
				addr = 0x25;
				break;
			case 2:
				addr = 0x27;
				break;
			}
			writeAXP(addr, (readAXP(addr) & 0x80) | (voltage & 0x7F));
		}

		void AXP192_SetLDOVoltage(uint8_t number, uint16_t voltage) {
			voltage = (voltage > 3300) ? 15 : (voltage / 100) - 18;
			if (2 == number) writeAXP(0x28, (readAXP(0x28) & 0x0F) | (voltage << 4));
			if (3 == number) writeAXP(0x28, (readAXP(0x28) & 0xF0) | voltage);
		}

		void AXP192_SetLDOEnable(uint8_t number, bool state) {
			uint8_t mark = 0x01;
			if ((number < 2) || (number > 3)) return;

			mark <<= number;
			if (state) {
				writeAXP(0x12, (readAXP(0x12) | mark));
			} else {
				writeAXP(0x12, (readAXP(0x12) & (~mark)));
			}
		}

		void AXP192_SetDCDC3(bool state) {
			uint8_t buf = readAXP(0x12);
			if (state == true) {
				buf = (1 << 1) | buf;
			} else {
				buf = ~(1 << 1) & buf;
			}
			writeAXP(0x12, buf);
		}

		void AXP192_SetLCDRSet(bool state) {
			uint8_t reg_addr = 0x96;
			uint8_t gpio_bit = 0x02;
			uint8_t data = readAXP(reg_addr);

			if (state) {
				data |= gpio_bit;
			} else {
				data &= ~gpio_bit;
			}
			writeAXP(reg_addr, data);
		}

		void AXP192_SetLed(uint8_t state) {
			uint8_t reg_addr = 0x94;
			uint8_t data = readAXP(reg_addr);

			if (state) {
				data = data & 0xFD;
			} else {
				data |= 0x02;
			}
			writeAXP(reg_addr, data);
		}

		void AXP192_SetSpkEnable(uint8_t state) {
			// Set true to enable speaker

			uint8_t reg_addr = 0x94;
			uint8_t gpio_bit = 0x04;
			uint8_t data;
			data = readAXP(reg_addr);

			if (state) {
				data |= gpio_bit;
			} else {
				data &= ~gpio_bit;
			}
			writeAXP(reg_addr, data);
		}

		void AXP192_SetCHGCurrent(uint8_t state) {
			uint8_t data = readAXP(0x33);
			data &= 0xf0;
			data = data | ( state & 0x0f );
			writeAXP(0x33, data);
		}

		void AXP192_SetBusPowerMode(uint8_t state) {
			// Select source for BUS_5V
			// 0 : powered by USB or battery; use internal boost
			// 1 : powered externally

			uint8_t data;
			if (state == 0) {
				// Set GPIO to 3.3V (LDO OUTPUT mode)
				data = readAXP(0x91);
				writeAXP(0x91, (data & 0x0F) | 0xF0);
				// Set GPIO0 to LDO OUTPUT, pullup N_VBUSEN to disable VBUS supply from BUS_5V
				data = readAXP(0x90);
				writeAXP(0x90, (data & 0xF8) | 0x02);
				// Set EXTEN to enable 5v boost
				data = readAXP(0x10);
				writeAXP(0x10, data | 0x04);
			} else {
				// Set EXTEN to disable 5v boost
				data = readAXP(0x10);
				writeAXP(0x10, data & ~0x04);
				// Set GPIO0 to float, using enternal pulldown resistor to enable VBUS supply from BUS_5V
				data = readAXP(0x90);
				writeAXP(0x90, (data & 0xF8) | 0x07);
			}
		}

		void AXP192_begin() {
			// derived from AXP192.cpp from https://github.com/m5stack/M5Core2
			Wire1.begin(21, 22);
			Wire1.setClock(400000);

			writeAXP(0x30, (readAXP(0x30) & 0x04) | 0x02); // turn vbus limit off
			writeAXP(0x92, readAXP(0x92) & 0xf8); // set gpio1 to output
			writeAXP(0x93, readAXP(0x93) & 0xf8); // set gpio2 to output
			writeAXP(0x35, (readAXP(0x35) & 0x1c) | 0xa2); // enable rtc battery charging
			AXP192_SetDCVoltage(0, 3350); // set esp32 power voltage to 3.35v
			AXP192_SetDCVoltage(2, 2800); // set backlight voltage was set to 2.8v
			AXP192_SetLDOVoltage(2, 3300); // set peripheral voltage (LCD_logic, SD card) voltage to 2.0v
			AXP192_SetLDOVoltage(3, 2000); // set vibrator motor voltage to 2.0v
			AXP192_SetLDOEnable(2, true);
			AXP192_SetDCDC3(true); // LCD backlight
			AXP192_SetLed(false);
			AXP192_SetSpkEnable(true);

			AXP192_SetCHGCurrent(0); // charge current: 100mA
			writeAXP(0x95, (readAXP(0x95) & 0x72) | 0x84); // GPIO4

			writeAXP(0x36, 0x4C); // ???
			writeAXP(0x82,0xff); // ???

			AXP192_SetLCDRSet(0);
			delay(100);
			AXP192_SetLCDRSet(1);
			delay(100);

			// axp: check v-bus status
			if (readAXP(0x00) & 0x08) {
				writeAXP(0x30, readAXP(0x30) | 0x80);
				// if has v-bus power, disable M-Bus 5V output to input
				AXP192_SetBusPowerMode(1);
			} else {
				// otherwise, enable M-Bus 5V output
				AXP192_SetBusPowerMode(0);
			}
		}

		void tftInit() {
			AXP192_begin();

			tft.begin(40000000); // Run SPI at 80MHz/2
			tft.setRotation(1);
			tft.invertDisplay(true);
			uint8_t m = 0x08 | 0x04; // RGB pixel order, refresh LCD right to left
			tft.sendCommand(ILI9341_MADCTL, &m, 1);

			tftClear();
			useTFT = true;
		}

		// M5 Core2 touchscreen support

		#define HAS_TOUCH_SCREEN 1
		#define CORE2_TOUCH_SCREEN_ADDR 0x38
		#define CORE2_SCREEN_TOUCHED_PIN 39

		static void setCore2TouchScreenReg(int regID, int value) {
			Wire1.beginTransmission(CORE2_TOUCH_SCREEN_ADDR);
			Wire1.write(regID);
			Wire1.write(value);
			Wire1.endTransmission();
		}

		static void touchInit() {
			setCore2TouchScreenReg(0xA4, 0); // hold TOUCHED_PIN low while screen touched
			pinMode(CORE2_SCREEN_TOUCHED_PIN, INPUT);
			touchEnabled = true;
		}

		static uint32 lastTouchUpdate = 0;
		static int touchScreenX = -1;
		static int touchScreenY = -1;

		static int screenTouched() {
			if (!touchEnabled) touchInit();
			return !digitalRead(CORE2_SCREEN_TOUCHED_PIN);
		}

		static void touchUpdate() {
			if (!touchEnabled) touchInit();
			uint32 now = millisecs();
			if ((now - lastTouchUpdate) < 10) return;
			if (screenTouched()) {
				uint8 data[4];
				Wire1.beginTransmission(CORE2_TOUCH_SCREEN_ADDR);
				Wire1.write(3);
				Wire1.endTransmission();
				Wire1.requestFrom(CORE2_TOUCH_SCREEN_ADDR, sizeof(data));
				for (int i = 0; i < sizeof(data); i++) {
					data[i] = Wire1.read();
				}
				touchScreenX = ((data[0] & 0xF) << 8) | data[1];
				touchScreenY = ((data[2] & 0xF) << 8) | data[3];
			} else {
				touchScreenX = -1;
				touchScreenY = -1;
			}
			lastTouchUpdate = now;
		}

		static int screenTouchX() {
			touchUpdate();
			return touchScreenX;
		}

		static int screenTouchY() {
			touchUpdate();
			return touchScreenY;
		}

		static int screenTouchPressure() {
			// pressure not supported; return a constant value if screen is touched, -1 if not
			if (!touchEnabled) touchInit();
			return screenTouched() ? 10 : -1;
		}
	#elif defined(ARDUINO_M5STACK_Core2) && defined(TFT_ESPI)
		// Preliminary: this is not yet working...
		#include <TFT_eSPI.h>

		// in User_Setup,h #define ILI9341_DRIVER
		// this definition also defines width and height.

		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object

//SPIClass mySPI(HSPI); 

		int readAXP(int reg) {
			Wire1.beginTransmission(0x34);
			Wire1.write(reg);
			Wire1.endTransmission();
			Wire1.requestFrom(0x34, 1);
			return Wire1.available() ? Wire1.read() : 0;
		}

		void writeAXP(int reg, int value) {
			Wire1.beginTransmission(0x34);
			Wire1.write(reg);
			Wire1.write(value);
			Wire1.endTransmission();
		}

		void AXP192_SetDCVoltage(uint8_t number, uint16_t voltage) {
			uint8_t addr;
			if (number > 2) return;
			voltage = (voltage < 700) ? 0 : (voltage - 700) / 25;
			switch (number) {
			case 0:
				addr = 0x26;
				break;
			case 1:
				addr = 0x25;
				break;
			case 2:
				addr = 0x27;
				break;
			}
			writeAXP(addr, (readAXP(addr) & 0x80) | (voltage & 0x7F));
		}

		void AXP192_SetLDOVoltage(uint8_t number, uint16_t voltage) {
			voltage = (voltage > 3300) ? 15 : (voltage / 100) - 18;
			if (2 == number) writeAXP(0x28, (readAXP(0x28) & 0x0F) | (voltage << 4));
			if (3 == number) writeAXP(0x28, (readAXP(0x28) & 0xF0) | voltage);
		}

		void AXP192_SetLDOEnable(uint8_t number, bool state) {
			uint8_t mark = 0x01;
			if ((number < 2) || (number > 3)) return;

			mark <<= number;
			if (state) {
				writeAXP(0x12, (readAXP(0x12) | mark));
			} else {
				writeAXP(0x12, (readAXP(0x12) & (~mark)));
			}
		}

		void AXP192_SetDCDC3(bool state) {
			uint8_t buf = readAXP(0x12);
			if (state == true) {
				buf = (1 << 1) | buf;
			} else {
				buf = ~(1 << 1) & buf;
			}
			writeAXP(0x12, buf);
		}

		void AXP192_SetLCDRSet(bool state) {
			uint8_t reg_addr = 0x96;
			uint8_t gpio_bit = 0x02;
			uint8_t data = readAXP(reg_addr);

			if (state) {
				data |= gpio_bit;
			} else {
				data &= ~gpio_bit;
			}
			writeAXP(reg_addr, data);
		}

		void AXP192_SetLed(uint8_t state) {
			uint8_t reg_addr = 0x94;
			uint8_t data = readAXP(reg_addr);

			if (state) {
				data = data & 0xFD;
			} else {
				data |= 0x02;
			}
			writeAXP(reg_addr, data);
		}

		void AXP192_SetSpkEnable(uint8_t state) {
			// Set true to enable speaker

			uint8_t reg_addr = 0x94;
			uint8_t gpio_bit = 0x04;
			uint8_t data;
			data = readAXP(reg_addr);

			if (state) {
				data |= gpio_bit;
			} else {
				data &= ~gpio_bit;
			}
			writeAXP(reg_addr, data);
		}

		void AXP192_SetCHGCurrent(uint8_t state) {
			uint8_t data = readAXP(0x33);
			data &= 0xf0;
			data = data | ( state & 0x0f );
			writeAXP(0x33, data);
		}

		void AXP192_SetBusPowerMode(uint8_t state) {
			// Select source for BUS_5V
			// 0 : powered by USB or battery; use internal boost
			// 1 : powered externally

			uint8_t data;
			if (state == 0) {
				// Set GPIO to 3.3V (LDO OUTPUT mode)
				data = readAXP(0x91);
				writeAXP(0x91, (data & 0x0F) | 0xF0);
				// Set GPIO0 to LDO OUTPUT, pullup N_VBUSEN to disable VBUS supply from BUS_5V
				data = readAXP(0x90);
				writeAXP(0x90, (data & 0xF8) | 0x02);
				// Set EXTEN to enable 5v boost
				data = readAXP(0x10);
				writeAXP(0x10, data | 0x04);
			} else {
				// Set EXTEN to disable 5v boost
				data = readAXP(0x10);
				writeAXP(0x10, data & ~0x04);
				// Set GPIO0 to float, using enternal pulldown resistor to enable VBUS supply from BUS_5V
				data = readAXP(0x90);
				writeAXP(0x90, (data & 0xF8) | 0x07);
			}
		}

		void AXP192_begin() {
			// derived from AXP192.cpp from https://github.com/m5stack/M5Core2
			Wire1.begin(21, 22);
			Wire1.setClock(400000);

			writeAXP(0x30, (readAXP(0x30) & 0x04) | 0x02); // turn vbus limit off
			writeAXP(0x92, readAXP(0x92) & 0xf8); // set gpio1 to output
			writeAXP(0x93, readAXP(0x93) & 0xf8); // set gpio2 to output
			writeAXP(0x35, (readAXP(0x35) & 0x1c) | 0xa2); // enable rtc battery charging
			AXP192_SetDCVoltage(0, 3350); // set esp32 power voltage to 3.35v
			AXP192_SetDCVoltage(2, 2800); // set backlight voltage was set to 2.8v
			AXP192_SetLDOVoltage(2, 3300); // set peripheral voltage (LCD_logic, SD card) voltage to 2.0v
			AXP192_SetLDOVoltage(3, 2000); // set vibrator motor voltage to 2.0v
			AXP192_SetLDOEnable(2, true);
			AXP192_SetDCDC3(true); // LCD backlight
			AXP192_SetLed(false);
			AXP192_SetSpkEnable(true);

			AXP192_SetCHGCurrent(0); // charge current: 100mA
			writeAXP(0x95, (readAXP(0x95) & 0x72) | 0x84); // GPIO4

			writeAXP(0x36, 0x4C); // ???
			writeAXP(0x82,0xff); // ???

			AXP192_SetLCDRSet(0);
			delay(100);
			AXP192_SetLCDRSet(1);
			delay(100);

			// axp: check v-bus status
			if (readAXP(0x00) & 0x08) {
				writeAXP(0x30, readAXP(0x30) | 0x80);
				// if has v-bus power, disable M-Bus 5V output to input
				AXP192_SetBusPowerMode(1);
			} else {
				// otherwise, enable M-Bus 5V output
				AXP192_SetBusPowerMode(0);
			}
		}

		void tftInit() {
			AXP192_begin();
//------------

		
			tft.init();
			tft.initDMA();
			//tft.setSwapBytes(true);
		
			tft.begin();
			tft.setRotation(3);
			tft.invertDisplay(true);

			//uint8_t m = 0x08 | 0x04; // RGB pixel order, refresh LCD right to left
			//tft.sendCommand(ILI9341_MADCTL, &m, 1);
			#define ILI9341_MADCTL 0x36

			uint8_t m = 0b00001000;  // Example MADCTL value (bit 3 = bgr)
			tft.writecommand(ILI9341_MADCTL);
			tft.writedata(m);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
		
			useTFT = true;
	}

		// M5 Core2 touchscreen support

		#define HAS_TOUCH_SCREEN 1
		#define CORE2_TOUCH_SCREEN_ADDR 0x38
		#define CORE2_SCREEN_TOUCHED_PIN 39

		static void setCore2TouchScreenReg(int regID, int value) {
			Wire1.beginTransmission(CORE2_TOUCH_SCREEN_ADDR);
			Wire1.write(regID);
			Wire1.write(value);
			Wire1.endTransmission();
		}

		static void touchInit() {
			setCore2TouchScreenReg(0xA4, 0); // hold TOUCHED_PIN low while screen touched
			pinMode(CORE2_SCREEN_TOUCHED_PIN, INPUT);
			touchEnabled = true;
		}

		static uint32 lastTouchUpdate = 0;
		static int touchScreenX = -1;
		static int touchScreenY = -1;

		static int screenTouched() {
			if (!touchEnabled) touchInit();
			return !digitalRead(CORE2_SCREEN_TOUCHED_PIN);
		}

		static void touchUpdate() {
			if (!touchEnabled) touchInit();
			uint32 now = millisecs();
			if ((now - lastTouchUpdate) < 10) return;
			if (screenTouched()) {
				uint8 data[4];
				Wire1.beginTransmission(CORE2_TOUCH_SCREEN_ADDR);
				Wire1.write(3);
				Wire1.endTransmission();
				Wire1.requestFrom(CORE2_TOUCH_SCREEN_ADDR, sizeof(data));
				for (int i = 0; i < sizeof(data); i++) {
					data[i] = Wire1.read();
				}
				touchScreenX = ((data[0] & 0xF) << 8) | data[1];
				touchScreenY = ((data[2] & 0xF) << 8) | data[3];
			} else {
				touchScreenX = -1;
				touchScreenY = -1;
			}
			lastTouchUpdate = now;
		}

		static int screenTouchX() {
			touchUpdate();
			return touchScreenX;
		}

		static int screenTouchY() {
			touchUpdate();
			return touchScreenY;
		}

		static int screenTouchPressure() {
			// pressure not supported; return a constant value if screen is touched, -1 if not
			if (!touchEnabled) touchInit();
			return screenTouched() ? 10 : -1;
		}

	#elif defined(ARDUINO_NRF52840_CLUE)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7789.h"

		#define TFT_CS		31
		#define TFT_DC		32
		#define TFT_RST		33
		#define TFT_WIDTH	240
		#define TFT_HEIGHT	240
		Adafruit_ST7789 tft = Adafruit_ST7789(&SPI1, TFT_CS, TFT_DC, TFT_RST);

		void tftInit() {
			tft.init(240, 240);
			tft.setRotation(1);
			tft.fillScreen(0);
			uint8_t rtna = 0x01; // Screen refresh rate control (datasheet 9.2.18, FRCTRL2)
			tft.sendCommand(0xC6, &rtna, 1);

			// fix for display gamma glitch on some Clue boards:
			uint8_t gamma = 2;
			tft.sendCommand(0x26, &gamma, 1);

			// Turn on backlight
			pinMode(34, OUTPUT);
			digitalWrite(34, HIGH);

			useTFT = true;
		}




	#elif defined(CYDS343) && !defined(LVGL)


	    // change: #if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR <3>)
		// in Arduino_ESP32RGBPanel.h

		#include <Arduino_GFX_Library.h>

#define GFX_BL 2
// option 1:
// Uncomment for ILI6485 LCD 480x272
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 1 /* hsync_front_porch */, 1 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    0 /* vsync_polarity */, 3 /* vsync_front_porch */, 1 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 9000000 /* prefer_speed */);



Arduino_RGB_Display tft = Arduino_RGB_Display(
    480 /* width */, 272 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);
// option 2:
// Uncomment for ST7262 IPS LCD 800x480
// Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
//     40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
//     45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
//     5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
//     8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
//     0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
//     0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
//     1 /* pclk_active_neg */, 16000000 /* prefer_speed */);
// Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
//     800 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);
// option 3:
// Uncomment for RPi DPI 1024x600
// Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
//     40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
//     45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
//     5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
//     8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
//     0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
//     0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
//     1 /* pclk_active_neg */, 9000000 /* prefer_speed */);
// Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
//     1024 /* width */, 600 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);

#define TFT_WIDTH 480
#define TFT_HEIGHT 272

void tftInit() {

	
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  //Serial.println("Arduino_GFX Hello World example");

  // Init Display
  tft.begin();
  tft.fillScreen(RGB565_BLACK);
  useTFT = true;
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

}

		// #include "touch.h"
		// #define HAS_TOUCH_SCREEN 1

		// static void touchInit() {
		//  	touch_init();
		// 	touchEnabled = true;
		// }

		// static uint32 lastTouchUpdate = 0;
		// static int touchScreenX = -1;
		// static int touchScreenY = -1;

		// static int screenTouched() {
		// 	if (!touchEnabled) touchInit();
		// 	bool is_touched = touch_touched();
		// 	return is_touched;
		// }

		// static void touchUpdate() {

		// 	if (!touchEnabled) touchInit();
		// 	uint32 now = millisecs();
		// 	if ((now - lastTouchUpdate) < 10) return;
		// 	if (screenTouched()) {
		// 		touchScreenX = touch_last_x;
		// 		touchScreenY = touch_last_y;
		// 	} 
		// 	/*else {
		// 		touchScreenX = -1;
		// 		touchScreenY = -1;
		// 	}*/
		// 	lastTouchUpdate = now;
		// }

		// static int screenTouchX() {
		// 	touchUpdate();
		// 	return touchScreenX;
		// }

		// static int screenTouchY() {
		// 	touchUpdate();
		// 	return touchScreenY;
		// }

		// static int screenTouchPressure() {
		// 	// pressure not supported; return a constant value if screen is touched, -1 if not
		// 	if (!touchEnabled) touchInit();
		// 	return screenTouched() ? 10 : -1;
		// }
	#elif defined(S3_CTF)
	bool flip_x_y=false;
		bool flip_x=false;
		bool flip_y=false;
		#define HAS_TOUCH_SCREEN 1
		#include <TFT_eSPI.h>

		// in User_Setup,h #define ILI9341_DRIVER
		// this definition also defines width and height.

		#include <XPT2046_Touchscreen.h>
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
		XPT2046_Touchscreen ts(TOUCH_CS);
		SPIClass& spix = SPI;
//SPIClass mySPI(HSPI); 

		void tftInit() {
			
			tft.init();
			tft.initDMA();
			//tft.setSwapBytes(true);
			spix = tft.getSPIinstance(); 
			//tft.fillScreen(TFT_BLACK);
		
			tft.begin();
			tft.setRotation(3);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			
			pinMode(39, OUTPUT);
			digitalWrite(39, HIGH);
			useTFT = true;
	}
  
	static void touchInit() {
		
		ts.begin(spix);
		//pinMode(TOUCH_CS, INPUT);
		//mySPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
		//ts.begin(mySPI);
  		ts.setRotation(3);
		//ts.setCalibration(X_MIN, X_MAX, Y_MIN, Y_MAX);
		//ts.setRotation(1);
		touchEnabled = true;
	}

	static int screenTouched() {
		if (!touchEnabled) touchInit();
		// char s[100];
		// sprintf(s,"touch init: %d ",ts.touched());
		// outputString(s);
		return ts.touched();
	}

	static int screenTouchX() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t x;
		if (flip_x_y) {
			x = ts.getPoint().y;
		} else
		  	x = ts.getPoint().x;
		// char s[100];
		// sprintf(s,"touch x: %d ",x);
		// outputString(s);
		if (flip_x) 
		   return map(x, 200, 3800, 0, TFT_HEIGHT);
		else
		   return map(x, 200, 3800, TFT_HEIGHT,0);
	}
		
	static int screenTouchY() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t y;
		if (flip_x_y) 
			y = ts.getPoint().x;
		else
			y = ts.getPoint().y;
		// char s[100];
		// sprintf(s,"touch y: %d ",y);
		// outputString(s);
		if (flip_y) 
			return  map(y, 300, 3900, TFT_WIDTH,0);
		else
			return  map(y, 300, 3900, 0, TFT_WIDTH);
		}
		
	static int screenTouchPressure() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		return ts.getPoint().z;
		}
		


static OBJ primfliptouch(int argCount, OBJ *args) {
	flip_x = (trueObj == args[0]);
	flip_y = (trueObj == args[1]);
	flip_x_y = (trueObj == args[2]);
return falseObj;
}
	#elif defined(LMSDISPLAY) && defined(TFT_ESPI)
		bool flip_x_y=false;
		bool flip_x=false;
		bool flip_y=false;
		#define HAS_TOUCH_SCREEN 1
		#include <TFT_eSPI.h>

		// in User_Setup,h #define ILI9341_DRIVER
		// this definition also defines width and height.

		#include <XPT2046_Touchscreen.h>
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
		XPT2046_Touchscreen ts(TOUCH_CS);
		SPIClass& spix = SPI;
//SPIClass mySPI(HSPI); 

		void tftInit() {
			
			tft.init();
			tft.initDMA();
			//tft.setSwapBytes(true);
			spix = tft.getSPIinstance(); 
			//tft.fillScreen(TFT_BLACK);
		
			tft.begin();
			tft.setRotation(3);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			
			pinMode(33, OUTPUT);
			digitalWrite(33, HIGH);
			useTFT = true;
	}
  
	static void touchInit() {
		
		ts.begin(spix);
		//pinMode(TOUCH_CS, INPUT);
		//mySPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
		//ts.begin(mySPI);
  		ts.setRotation(3);
		//ts.setCalibration(X_MIN, X_MAX, Y_MIN, Y_MAX);
		//ts.setRotation(1);
		touchEnabled = true;
	}

	static int screenTouched() {
		if (!touchEnabled) touchInit();
		// char s[100];
		// sprintf(s,"touch init: %d ",ts.touched());
		// outputString(s);
		return ts.touched();
	}

	static int screenTouchX() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t x;
		if (flip_x_y) {
			x = ts.getPoint().y;
		} else
		  	x = ts.getPoint().x;
		// char s[100];
		// sprintf(s,"touch x: %d ",x);
		// outputString(s);
		if (flip_x) 
		   return map(x, 200, 3800, 0, TFT_HEIGHT);
		else
		   return map(x, 200, 3800, TFT_HEIGHT,0);
	}
		
	static int screenTouchY() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t y;
		if (flip_x_y) 
			y = ts.getPoint().x;
		else
			y = ts.getPoint().y;
		// char s[100];
		// sprintf(s,"touch y: %d ",y);
		// outputString(s);
		if (flip_y) 
			return  map(y, 300, 3900, TFT_WIDTH,0);
		else
			return  map(y, 300, 3900, 0, TFT_WIDTH);
		}
		
	static int screenTouchPressure() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		return ts.getPoint().z;
		}
		


static OBJ primfliptouch(int argCount, OBJ *args) {
	flip_x = (trueObj == args[0]);
	flip_y = (trueObj == args[1]);
	flip_x_y = (trueObj == args[2]);
return falseObj;
}

#elif defined(CYDS343) && defined(LVGL)


// change: #if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR <3>)
// in Arduino_ESP32RGBPanel.h

#include <Arduino_GFX_Library.h>

// sodb: change this line in .pio/
/*

in Arduino_ESP32RGBPanel.h

#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR < 3)
//#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR >5)


*/





#define GFX_BL 2
// option 1:
// Uncomment for ILI6485 LCD 480x272
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 1 /* hsync_front_porch */, 1 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    0 /* vsync_polarity */, 3 /* vsync_front_porch */, 1 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 9000000 /* prefer_speed */);



Arduino_RGB_Display tft = Arduino_RGB_Display(
    480 /* width */, 272 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);


#define TFT_WIDTH 480
#define TFT_HEIGHT 272

void tftInit() {

	
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  //Serial.println("Arduino_GFX Hello World example");

  // Init Display
  tft.begin();
  tft.fillScreen(RGB565_BLACK);
  useTFT = true;
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

}

#if defined(BBCT)

#include <bb_captouch.h>
#include <Wire.h>
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_INT -1
#define TOUCH_GT911_RST 38

BBCapTouch bbct;

  


   	#define HAS_TOUCH_SCREEN 1

		static void touchInit() {

			Wire1.end();
			bbct.init(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_RST, TOUCH_GT911_INT, 400000, &Wire1);
			int iType = bbct.sensorType();
			const char *szNames[] = {"Unknown", "FT6x36", "GT911", "CST820", "CST226", "MXT144", "AXS15231"};
			char s[100];
			sprintf(s,"sensor type: %s", szNames[iType] );
			outputString(s);
				
			
			touchEnabled = true;
		}

		static uint32 lastTouchUpdate = 0;
		static int touchScreenX = -1;
		static int touchScreenY = -1;
		static int touchSize =0;
		
		static int screenTouched() {
			if (!touchEnabled) touchInit();
			 TOUCHINFO ti;
			if (bbct.getSamples(&ti)) {
				touchScreenX = ti.x[0];
      			touchScreenY = ti.y[0];
 				touchSize = ti.area[0];
				return true;
			} else
			return false;
		}

		static void touchUpdate() {

			if (!touchEnabled) touchInit();
			screenTouched();	  
			//uint32 now = millisecs();
			//if ((now - lastTouchUpdate) < 10) return;
			// if (screenTouched()) {
			// 	touchScreenX = ts.points[0].x;;
			// 	touchScreenY = ts.points[0].y;
			// 	touchSize = ts.points[0].size;
			// } 
			//lastTouchUpdate = now;
		
		}

		static int screenTouchX() {
			touchUpdate();
			return touchScreenX;
		}

		static int screenTouchY() {
			touchUpdate();
			return touchScreenY;
		}

		static int screenTouchPressure() {
			// pressure not supported; return a constant value if screen is touched, -1 if not
			return touchSize;
		}



#else
#include <TAMC_GT911.h>		
// Setup touch
#define TOUCH_GT911
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_INT -1
#define TOUCH_GT911_RST 38
#define TOUCH_GT911_ROTATION ROTATION_RIGHT  //ROTATION_NORMAL
#define TOUCH_MAP_X1 480
#define TOUCH_MAP_X2 0
#define TOUCH_MAP_Y1 272
#define TOUCH_MAP_Y2 0

TAMC_GT911 ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

		#define HAS_TOUCH_SCREEN 1

		static void touchInit() {
		 	//touch_init();
			ts.begin();
  			ts.setRotation(1);
			touchEnabled = true;
		}

		static uint32 lastTouchUpdate = 0;
		static int touchScreenX = -1;
		static int touchScreenY = -1;
		static int touchSize =0;
		
		static int screenTouched() {
			if (!touchEnabled) touchInit();
			ts.read();
			touchScreenX = ts.points[0].x;;
			touchScreenY = ts.points[0].y;
			touchSize = ts.points[0].size;
			return (ts.isTouched);
		}

		static void touchUpdate() {

			if (!touchEnabled) touchInit();
			  
			//uint32 now = millisecs();
			//if ((now - lastTouchUpdate) < 10) return;
			// if (screenTouched()) {
			// 	touchScreenX = ts.points[0].x;;
			// 	touchScreenY = ts.points[0].y;
			// 	touchSize = ts.points[0].size;
			// } 
			//lastTouchUpdate = now;
		
		}

		static int screenTouchX() {
			touchUpdate();
			return touchScreenX;
		}

		static int screenTouchY() {
			touchUpdate();
			return touchScreenY;
		}

		static int screenTouchPressure() {
			// pressure not supported; return a constant value if screen is touched, -1 if not
			int pressure=0;
			if (ts.touches>0) pressure = ts.points[0].size;
			return pressure;
		}
#endif
	#elif defined(LMSDISPLAY) && defined(TFT_ESPI)
		bool flip_x_y=false;
		bool flip_x=false;
		bool flip_y=false;
		#define HAS_TOUCH_SCREEN 1
		#include <TFT_eSPI.h>

		// in User_Setup,h #define ILI9341_DRIVER
		// this definition also defines width and height.

		#include <XPT2046_Touchscreen.h>
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
		XPT2046_Touchscreen ts(TOUCH_CS);
		SPIClass& spix = SPI;
//SPIClass mySPI(HSPI); 

		void tftInit() {
			
			tft.init();
			tft.initDMA();
			//tft.setSwapBytes(true);
			spix = tft.getSPIinstance(); 
			//tft.fillScreen(TFT_BLACK);
		
			tft.begin();
			tft.setRotation(3);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			
			pinMode(33, OUTPUT);
			digitalWrite(33, HIGH);
			useTFT = true;
	}
  
	static void touchInit() {
		
		ts.begin(spix);
		//pinMode(TOUCH_CS, INPUT);
		//mySPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
		//ts.begin(mySPI);
  		ts.setRotation(3);
		//ts.setCalibration(X_MIN, X_MAX, Y_MIN, Y_MAX);
		//ts.setRotation(1);
		touchEnabled = true;
	}

	static int screenTouched() {
		if (!touchEnabled) touchInit();
		// char s[100];
		// sprintf(s,"touch init: %d ",ts.touched());
		// outputString(s);
		return ts.touched();
	}

	static int screenTouchX() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t x;
		if (flip_x_y) {
			x = ts.getPoint().y;
		} else
		  	x = ts.getPoint().x;
		// char s[100];
		// sprintf(s,"touch x: %d ",x);
		// outputString(s);
		if (flip_x) 
		   return map(x, 200, 3800, 0, TFT_HEIGHT);
		else
		   return map(x, 200, 3800, TFT_HEIGHT,0);
	}
		
	static int screenTouchY() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t y;
		if (flip_x_y) 
			y = ts.getPoint().x;
		else
			y = ts.getPoint().y;
		// char s[100];
		// sprintf(s,"touch y: %d ",y);
		// outputString(s);
		if (flip_y) 
			return  map(y, 300, 3900, TFT_WIDTH,0);
		else
			return  map(y, 300, 3900, 0, TFT_WIDTH);
		}
		
	static int screenTouchPressure() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		return ts.getPoint().z;
		}
		


static OBJ primfliptouch(int argCount, OBJ *args) {
	flip_x = (trueObj == args[0]);
	flip_y = (trueObj == args[1]);
	flip_x_y = (trueObj == args[2]);
return falseObj;
}




	#elif defined(CYDR) && defined(TFT_ESPI)
		bool flip_x_y=false;
		bool flip_x=true;
		bool flip_y=false;
		#define HAS_TOUCH_SCREEN 1
		#include <TFT_eSPI.h>

		// in User_Setup,h #define ILI9341_DRIVER
		// this definition also defines width and height.

		#include <XPT2046_Touchscreen.h>
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
		
		#if defined(CYDROT) 
// 14 -> tp_clk
// 33 > tp_cs
// 13 -> din
// 12 -> dout
// 39 irq
		#define XPT2046_IRQ 39
		#define XPT2046_CS 33
		XPT2046_Touchscreen ts(TOUCH_CS);
		SPIClass& spix = SPI;
#define TFT_HOR_RES   320
#define TFT_VER_RES   240
     #else


		#define XPT2046_IRQ 36
	#define XPT2046_MOSI 32
	#define XPT2046_MISO 39
	#define XPT2046_CLK 25
	#define XPT2046_CS 33
	SPIClass touchscreenSpi = SPIClass(VSPI);
	XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
	#define TFT_HOR_RES   240
#define TFT_VER_RES   320
#endif

uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240,touchScreenMaximumY = 3800;

/*Set to your screen resolution*/




		void tftInit() {
			
			tft.init();
			tft.initDMA();
			//tft.setSwapBytes(true);
			#if defined(CYDROT)
				spix = tft.getSPIinstance(); 
			#endif

			//tft.fillScreen(TFT_BLACK);
		
			tft.begin();
			#if defined(USB2)
				tft.invertDisplay(false); 
			#endif
			#if defined(CYDROT)
				tft.setRotation(2);
				tft.writecommand(ILI9341_MADCTL);
    		    tft.writedata(0x48);  // try 0x48, 0x28, 0x88, 0xE8 depending on rotation
			#else
				tft.setRotation(3);
			#endif
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			
			pinMode(TFT_BL, OUTPUT);
			digitalWrite(TFT_BL, HIGH);
			useTFT = true;
	}
  
	static void touchInit() {
		#if defined(CYDROT)
			ts.begin(spix);
			ts.setRotation(2); /* Inverted landscape orientation to match screen */
		#else

			touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); /* Start second SPI bus for touchscreen */
  			ts.begin(touchscreenSpi); /* Touchscreen init */
  			ts.setRotation(3); /* Inverted landscape orientation to match screen */
		#endif
		
		touchEnabled = true;
	}

	static int screenTouched() {
		if (!touchEnabled) touchInit();
		// char s[100];
		// sprintf(s,"touch init: %d ",ts.touched());
		// outputString(s);
		return ts.touched();
	}

	static int screenTouchX() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t x;
		if (flip_x_y) {
			x = ts.getPoint().y;
		} else
		  	x = ts.getPoint().x;
		// char s[100];
		// sprintf(s,"touch x: %d ",x);
		// outputString(s);
		if (flip_x) 
		   return map(x, 200, 3800, 0, TFT_HEIGHT);
		else
		   return map(x, 200, 3800, TFT_HEIGHT,0);
	}
		
	static int screenTouchY() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t y;
		if (flip_x_y) 
			y = ts.getPoint().x;
		else
			y = ts.getPoint().y;
		// char s[100];
		// sprintf(s,"touch y: %d ",y);
		// outputString(s);
		if (flip_y) 
			return  map(y, 300, 3900, TFT_WIDTH,0);
		else
			return  map(y, 300, 3900, 0, TFT_WIDTH);
		}
		
	static int screenTouchPressure() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		return ts.getPoint().z;
		}
		


static OBJ primfliptouch(int argCount, OBJ *args) {
	flip_x = (trueObj == args[0]);
	flip_y = (trueObj == args[1]);
	flip_x_y = (trueObj == args[2]);
return falseObj;
}



 #elif defined(CYDC) && defined(TFT_ESPI)
	#define HAS_TOUCH_SCREEN 1
		#include <TFT_eSPI.h>

	
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
	
	#define TOUCH_I2C_SDA 33
	#define TOUCH_I2C_SCL 32
	#define CST820_ADDR 0x15


		void tftInit() {
			
			tft.init();
			tft.initDMA();
		
			tft.begin();
			tft.setRotation(3);
			//tft.setViewport(0, 20, 240, 300);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			pinMode(27, OUTPUT);
			digitalWrite(27, HIGH);
			outputString("TFT initialized");
			useTFT = true;
	}
  
	

	
	static int wireTouchStarted = false;

	static void startTouchWire() {
		//if (internalWireStarted) {
			Wire1.end(); // stop wire interfacebefore setting pins
			Wire1.setPins(TOUCH_I2C_SDA,TOUCH_I2C_SCL);
			Wire1.begin();
			Wire1.setClock(400000); 
			wireTouchStarted = true;
		//}
	}
	
	int readI2CTouchReg(int deviceID, int reg) {
		if (!wireTouchStarted) startTouchWire();
		if (!wireTouchStarted) return -100; // could not start I2C; missing pullup resistors?
	
		Wire1.beginTransmission(deviceID);
		Wire1.write(reg);
		int error = Wire1.endTransmission();
		if (error) return -error; // error; bad device ID?
	
		Wire1.requestFrom(deviceID, 1);
	
		return Wire1.available() ? Wire1.read() : 0;
	}
	


	void writeI2CTouchReg(int deviceID, int reg, int value) {
		if (!wireTouchStarted) startTouchWire();
		if (!wireTouchStarted) return;
	
		Wire1.beginTransmission(deviceID);
		Wire1.write(reg);
		Wire1.write(value);
		Wire1.endTransmission();
	}


	// uint8_t i2c_read(uint16_t addr, uint8_t reg_addr, uint8_t *reg_data, size_t length)
	// {
	// 	Wire1.beginTransmission(addr);
	// 	Wire1.write(reg_addr);
	// 	if ( Wire1.endTransmission(true))return -1;
	// 	Wire1.requestFrom(addr, length, true);
	// 	for (int i = 0; i < length; i++) {
	// 		*reg_data++ = Wire.read();
	// 	}
	// 	return 0;
	// }

	
	static void touchInit() {
		startTouchWire();
		writeI2CTouchReg(CST820_ADDR, 0xfe, 0xff); // do not sleep 
		touchEnabled = true;
	}

	static uint32 lastTouchUpdate = 0;
	static int touchScreenX = -1;
	static int touchScreenY = -1;
	static int touchGesture = -1;

    // https://github.com/fbiego/CST816S

	static int screenTouched() {
		touchInit();
 		Wire1.beginTransmission(CST820_ADDR);
  		Wire1.write(0x00);  // Read from register 0x00
  		if (Wire1.endTransmission(false) != 0) {
    		//outputString("CST820 not responding\n");
	    	return 0;
		}

		  Wire1.requestFrom(CST820_ADDR, 7);
  		if (Wire1.available() < 7) {
    		//outputString("Incomplete data\n");
    		return 0;
  		}

		
		if (!touchEnabled) touchInit();
		int points = readI2CTouchReg(CST820_ADDR, 2); // read touch count
		if (points < 1 || points > 5) { // something went wrong
			return 0;
		} else return points;

	}

	static void touchUpdate() {
		if (!touchEnabled) touchInit();
		uint32 now = millisecs();
		if ((now - lastTouchUpdate) < 10) return;
		if (screenTouched()) {
			uint8 data[6];
			Wire1.beginTransmission(CST820_ADDR);
			Wire1.write(1);
			Wire1.endTransmission();
			Wire1.requestFrom(CST820_ADDR, sizeof(data));
			for (int i = 0; i < sizeof(data); i++) {
				data[i] = Wire1.read();
			}
			touchGesture = data[0];
			touchScreenY = TFT_WIDTH - (((data[2] & 0xF) << 8) | data[3]);
			touchScreenX = ((data[4] & 0xF) << 8) | data[5];
			//char s[100];
			//sprintf(s,"touch ct820 gesture: %d x:%d y:%d\n",touchGesture,touchScreenX,touchScreenY);
			//outputString(s);
		} else {
			touchScreenX = -1;
			touchScreenY = -1;
		}
		lastTouchUpdate = now;
	}

	static int screenTouchX() {
		touchUpdate();
		return touchScreenX;
	}

	static int screenTouchY() {
		touchUpdate();
		return touchScreenY;
	}

	static int screenTouchGesture() {
		touchUpdate();
		return touchGesture;
	}

	static int screenTouchPressure() {
		// pressure not supported; return a constant value if screen is touched, -1 if not
		if (!touchEnabled) touchInit();
		return screenTouched() ? 1000 : -1;
	}



    #elif defined(LMSDISPLAY) && !defined(TFT_ESPI)
	// disp = ili9341(miso=12, mosi=13, clk=14, cs=15, dc=27, rst=32, backlight=-1,power=-1,width=320, height=240, rot=LANDSCAPE)
    // touch = xpt2046(spihost=esp.HSPI_HOST,cs=26,transpose=False,cal_x0=3865, 
	#include "Adafruit_GFX.h"
	#include "Adafruit_ILI9341.h"
	
	#include <XPT2046_Touchscreen.h>
	#include <SPI.h>

	#define TFT_WIDTH 320
	#define TFT_HEIGHT 240


	#define HAS_TOUCH_SCREEN 1
	#define TOUCH_CS_PIN 26
	

	#define X_MIN 256
	#define X_MAX 3632
	#define Y_MIN 274
	#define Y_MAX 3579

	#define TFT_MOSI 13
	#define TFT_MISO 12
	#define TFT_SCLK 14
	#define TFT_CS 15
	#define TFT_DC 27
	#define TFT_RST 32

	SPIClass mySPI(HSPI); 
	

	Adafruit_ILI9341 tft = Adafruit_ILI9341(&mySPI, TFT_DC, TFT_CS, TFT_RST);
	XPT2046_Touchscreen ts( TOUCH_CS_PIN);
	void tftInit() {
		mySPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
	
		tft.begin(40000000);
		//tft.setRotation(0);
//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
		tftClear();
		// Turn on backlight on IoT-Bus
		pinMode(33, OUTPUT);
		digitalWrite(33, HIGH);

		useTFT = true;
		// enable lvgl in ioPrims at hardware initialization
		// #if defined(LVGL)
		//  setup_lvgl();
		// #endif


	}
  
	static void touchInit() {
		ts.begin(mySPI);
		//ts.setCalibration(X_MIN, X_MAX, Y_MIN, Y_MAX);
		//ts.setRotation(1);
		touchEnabled = true;
	}

	static int screenTouched() {
		if (!touchEnabled) touchInit();
		return ts.touched();
	}

	static int screenTouchX() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t x = ts.getPoint().x;
		return ((float)(x-X_MIN)/X_MAX)*TFT_WIDTH;
		}
		
	static int screenTouchY() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t y = ts.getPoint().y;
		return (1-((float)(y-Y_MIN)/Y_MAX))*TFT_HEIGHT;
		}
		
	static int screenTouchPressure() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		return ts.getPoint().z;
		}

#elif defined(UNIHIKER)


void pca9535_write(int reg, int value) {
			Wire.beginTransmission(0x20);
			Wire.write(reg);
			Wire.write(value);
			Wire.endTransmission();
		}

void pca9535_BL() {
			// derived from AXP192.cpp from https://github.com/m5stack/M5Core2
			//Wire.setPins(PIN_WIRE_SDA, PIN_WIRE_SCL);
			Wire.begin(47, 48);
			Wire1.setClock(400000);

			pca9535_write(0x6, 0); // Configuration Port 0, output
			pca9535_write(0x2, 1); // set P0 to output = BL
			Wire.end();
			
		}



	#include "Adafruit_GFX.h"
	#include "Adafruit_ILI9341.h"
	
	#include <SPI.h>

    #define TFT_MOSI 21
	#define TFT_MISO -1
	#define TFT_SCLK 12
	#define TFT_CS 14
	#define TFT_DC 13
	#define TFT_RST -1

	SPIClass mySPI(HSPI); 
	
	#define TFT_WIDTH 320
	#define TFT_HEIGHT 240
	Adafruit_ILI9341 tft = Adafruit_ILI9341(&mySPI, TFT_DC, TFT_CS, TFT_RST);
	//XPT2046_Touchscreen ts( TOUCH_CS_PIN);
	void tftInit() {
		mySPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
	
		tft.begin(80000000);
		tft.setRotation(3);
//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
		tftClear();
		// Turn on backlight on IoT-Bus
		//pinMode(33, OUTPUT);
		//digitalWrite(33, HIGH);
		pca9535_BL();
		useTFT = true;
	}
  
#elif defined(C3LVGL)
		
		#include <TFT_eSPI.h>

	
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
	#include "touch.h"	
	

		void tftInit() {
			
			tft.init();
			tft.initDMA();
		
			tft.begin();
			tft.setRotation(1);
			//tft.setViewport(0, 20, 240, 300);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			pinMode(TFT_BL, OUTPUT);
			digitalWrite(TFT_BL, HIGH);

			useTFT = true;
	}
  
/*
		#define HAS_TOUCH_SCREEN 1

		static void touchInit() {
		 	//touch_init();
			
			touchEnabled = true;
		}

		static uint32 lastTouchUpdate = 0;
		static int touchScreenX = -1;
		static int touchScreenY = -1;
		static int touchSize =0;
		
		static int screenTouched() {
			if (touch_touched()) {
				
				return true;
			}
			else
			return (false);
		}

		static void touchUpdate() {

			if (!touchEnabled) touchInit();
			touchScreenX = touch_last_x;
			touchScreenY =  touch_last_y;
			//uint32 now = millisecs();
			//if ((now - lastTouchUpdate) < 10) return;
			// if (screenTouched()) {
			// 	touchScreenX = ts.points[0].x;;
			// 	touchScreenY = ts.points[0].y;
			// 	touchSize = ts.points[0].size;
			// } 
			//lastTouchUpdate = now;
		
		}

		static int screenTouchX() {
			touchUpdate();
			return touchScreenX;
		}

		static int screenTouchY() {
			touchUpdate();
			return touchScreenY;
		}

		static int screenTouchPressure() {
			// pressure not supported; return a constant value if screen is touched, -1 if not
			int pressure=0;
			if (ts.touches>0) pressure = ts.points[0].size;
			return pressure;
		}
	*/

#elif defined(LMS7789)
#if !defined(TFT_ESPI)

	#include "Adafruit_GFX.h"
	#include "Adafruit_ST7789.h"
	
	#define HAS_TOUCH_SCREEN 1
	// sodb
	// HAS_INTERNAL_I2C defined in sensorPrims.cpp, running on Wire1
	#define TOUCH_I2C_SDA 32
	#define TOUCH_I2C_SCL 33
	#define CST820_ADDR 0x15

	#define TFT_MOSI 26
	#define TFT_MISO -1
	#define TFT_SCLK 15
	#define TFT_CS 14
	
	#define TFT_DC 27
	#define TFT_RST 13

	SPIClass mySPI(HSPI); 
	
	#define TFT_WIDTH 280
	#define TFT_HEIGHT 240
	Adafruit_ST7789 tft = Adafruit_ST7789(&mySPI, TFT_CS, TFT_DC, TFT_RST);

	void tftInit() {
		mySPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
		mySPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE3));
		//tft.begin(40000000);
		tft.init(TFT_HEIGHT, TFT_WIDTH);
		tft.setRotation(3);
//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
		tftClear();
		// Turn on backlight on IoT-Bus
		pinMode(12, OUTPUT);
		digitalWrite(12, HIGH);

		useTFT = true;
	}

#else 
		#define HAS_TOUCH_SCREEN 1
		#include <TFT_eSPI.h>

	
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
	
	#define TOUCH_I2C_SDA 32
	#define TOUCH_I2C_SCL 33
	#define CST820_ADDR 0x15


		void tftInit() {
			
			tft.init();
			tft.initDMA();
		
			tft.begin();
			tft.setRotation(1);
			//tft.setViewport(0, 20, 240, 300);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			pinMode(12, OUTPUT);
			digitalWrite(12, HIGH);

			useTFT = true;
	}
  
	
#endif	
	
	static int wireTouchStarted = false;

	static void startTouchWire() {
		//if (internalWireStarted) {
			Wire1.end(); // stop wire interfacebefore setting pins
			Wire1.setPins(TOUCH_I2C_SDA,TOUCH_I2C_SCL);
			Wire1.begin();
			Wire1.setClock(400000); 
			wireTouchStarted = true;
		//}
	}
	
	int readI2CTouchReg(int deviceID, int reg) {
		if (!wireTouchStarted) startTouchWire();
		if (!wireTouchStarted) return -100; // could not start I2C; missing pullup resistors?
	
		Wire1.beginTransmission(deviceID);
		Wire1.write(reg);
		int error = Wire1.endTransmission();
		if (error) return -error; // error; bad device ID?
	
		Wire1.requestFrom(deviceID, 1);
	
		return Wire1.available() ? Wire1.read() : 0;
	}
	


	void writeI2CTouchReg(int deviceID, int reg, int value) {
		if (!wireTouchStarted) startTouchWire();
		if (!wireTouchStarted) return;
	
		Wire1.beginTransmission(deviceID);
		Wire1.write(reg);
		Wire1.write(value);
		Wire1.endTransmission();
	}


	// uint8_t i2c_read(uint16_t addr, uint8_t reg_addr, uint8_t *reg_data, size_t length)
	// {
	// 	Wire1.beginTransmission(addr);
	// 	Wire1.write(reg_addr);
	// 	if ( Wire1.endTransmission(true))return -1;
	// 	Wire1.requestFrom(addr, length, true);
	// 	for (int i = 0; i < length; i++) {
	// 		*reg_data++ = Wire.read();
	// 	}
	// 	return 0;
	// }

	
	static void touchInit() {
		startTouchWire();
		writeI2CTouchReg(CST820_ADDR, 0xfe, 0xff); // do not sleep 
		touchEnabled = true;
	}

	static uint32 lastTouchUpdate = 0;
	static int touchScreenX = -1;
	static int touchScreenY = -1;
	static int touchGesture = -1;

    // https://github.com/fbiego/CST816S

	static int screenTouched() {
		touchInit();
 		Wire1.beginTransmission(CST820_ADDR);
  		Wire1.write(0x00);  // Read from register 0x00
  		if (Wire1.endTransmission(false) != 0) {
    		//outputString("CST820 not responding\n");
	    	return 0;
		}

		  Wire1.requestFrom(CST820_ADDR, 7);
  		if (Wire1.available() < 7) {
    		//outputString("Incomplete data\n");
    		return 0;
  		}

		
		if (!touchEnabled) touchInit();
		int points = readI2CTouchReg(CST820_ADDR, 2); // read touch count
		if (points < 1 || points > 5) { // something went wrong
			return 0;
		} else return points;

	}

	static void touchUpdate() {
		if (!touchEnabled) touchInit();
		uint32 now = millisecs();
		if ((now - lastTouchUpdate) < 10) return;
		if (screenTouched()) {
			uint8 data[6];
			Wire1.beginTransmission(CST820_ADDR);
			Wire1.write(1);
			Wire1.endTransmission();
			Wire1.requestFrom(CST820_ADDR, sizeof(data));
			for (int i = 0; i < sizeof(data); i++) {
				data[i] = Wire1.read();
			}
			touchGesture = data[0];
			touchScreenY = TFT_WIDTH - (((data[2] & 0xF) << 8) | data[3]);
			touchScreenX = ((data[4] & 0xF) << 8) | data[5];
			//char s[100];
			//sprintf(s,"touch ct820 gesture: %d x:%d y:%d\n",touchGesture,touchScreenX,touchScreenY);
			//outputString(s);
		} else {
			touchScreenX = -1;
			touchScreenY = -1;
		}
		lastTouchUpdate = now;
	}

	static int screenTouchX() {
		touchUpdate();
		return touchScreenX;
	}

	static int screenTouchY() {
		touchUpdate();
		return touchScreenY;
	}

	static int screenTouchGesture() {
		touchUpdate();
		return touchGesture;
	}

	static int screenTouchPressure() {
		// pressure not supported; return a constant value if screen is touched, -1 if not
		if (!touchEnabled) touchInit();
		return screenTouched() ? 1000 : -1;
	}


	#elif defined(ARDUINO_IOT_BUS) & !defined(TFT_ESPI)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ILI9341.h"
		#include <XPT2046_Touchscreen.h>
		#include <SPI.h>

		#define HAS_TOUCH_SCREEN 1
		#define TOUCH_CS_PIN 16
		XPT2046_Touchscreen ts(TOUCH_CS_PIN);

		#define X_MIN 256
		#define X_MAX 3632
		#define Y_MIN 274
		#define Y_MAX 3579

		#define TFT_CS	5
		#define TFT_DC	27

		#define TFT_WIDTH 320
		#define TFT_HEIGHT 240
		Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

		void tftInit() {
			tft.begin();
			tft.setRotation(1);
//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			pinMode(33, OUTPUT);
			digitalWrite(33, HIGH);

			useTFT = true;
		}

		static void touchInit() {
			ts.begin();
			ts.setRotation(1);
			touchEnabled = true;
		}

		static int screenTouched() {
			if (!touchEnabled) touchInit();
			return ts.touched();
		}

		static int screenTouchX() {
			if (!touchEnabled) touchInit();
			if (!ts.touched()) { return -1; }
			uint16_t x, y;
			uint8_t pressure;
			ts.readData(&x, &y, &pressure);
// 			x = (320 * (x - 256)) / 10;
// 			if (x < 0) x = 0;
// 			if (x > 320) x = 320;
			return x;
		}

		static int screenTouchY() {
			if (!touchEnabled) touchInit();
			if (!ts.touched()) { return -1; }
			uint16_t x, y;
			uint8_t pressure;
			ts.readData(&x, &y, &pressure);
// 			y = (240 * (y - 274)) / 14;
// 			if (y < 0) y = 0;
// 			if (y > 240) y = 240;
			return y;
		}

		static int screenTouchPressure() {
			if (!touchEnabled) touchInit();
			if (!ts.touched()) { return -1; }
			uint16_t x, y;
			uint8_t pressure;
			ts.readData(&x, &y, &pressure);
			return pressure;
		}

	#elif defined(SCOUT_MAKES_AZUL)
		#undef BLACK // defined in SSD1306 header
		#include "Adafruit_GFX.h"
		#include "Adafruit_SSD1306.h"

		#define TFT_WIDTH 128
		#define TFT_HEIGHT 32
		#define IS_MONOCHROME true

		Adafruit_SSD1306 tft = Adafruit_SSD1306(TFT_WIDTH, TFT_HEIGHT);

		#undef UPDATE_DISPLAY
		#define UPDATE_DISPLAY() { if (!deferUpdates) { tft.display(); taskSleep(-1); }}

		void tftInit() {
			tft.begin(SSD1306_SWITCHCAPVCC, 0x3C);
			useTFT = true;
			tftClear();
		}

	#elif defined(OLED_128_64)
		#undef BLACK // defined in SSD1306 header
		#undef WHITE // defined in SSD1306 header
		#include "Adafruit_GFX.h"
		#include "Adafruit_SSD1306.h"

		#define TFT_ADDR 0x3C
		#define TFT_WIDTH 128
		#define TFT_HEIGHT 64
		#define IS_MONOCHROME true

		Adafruit_SSD1306 tft = Adafruit_SSD1306(TFT_WIDTH, TFT_HEIGHT, &Wire, -1, 400000, 400000);

		static void oledCmd(uint8 cmd) {
			Wire.beginTransmission(TFT_ADDR);
			Wire.write(0x80);
			Wire.write(cmd);
			Wire.endTransmission(true);
		}

		void tftInit() {
			delay(5); // need 2 msecs minimum for micro:bit PicoBricks board power up I2C pullups
			if (!hasI2CPullups()) return; // no OLED connected and no I2C pullups

			// Ping the DUELink address so DUELink modules will use I2C mode
			// (This must be the first I2C transaction after power up.)
			readI2CReg(82, 0);

			int response = readI2CReg(TFT_ADDR, 0); // test if OLED responds at TFT_ADDR
			if (response < 0) return; // no OLED display detected
			isOLED1106 = (8 == (response & 15));

			tft.begin(SSD1306_SWITCHCAPVCC, TFT_ADDR);

			// set to medium brightness
			oledCmd(0x81);
			oledCmd(0x80);

			useTFT = true;
			tftClear();
		}

		static void i2cWriteBytes(uint8 *bytes, int byteCount) {
			Wire.beginTransmission(TFT_ADDR);
			for (int i = 0; i < byteCount; i++) Wire.write(bytes[i]);
			Wire.endTransmission(true);
		}

		static void oledUpdate() {
			// Send the entire OLED buffer to the display via i2c. Takes about 30 msecs.
			// Periodically update the LED display to avoid flicker.
			uint8 setupCmds[] = {
				0x20, 0,		// Horizontal mode
				0x21, 0, 0x7F,	// Column start and end address
				0x22, 0, 7		// Page start and end address
			};
			i2cWriteBytes(setupCmds, sizeof(setupCmds));
			uint8 buffer[65];
			buffer[0] = 0x40;
			uint8 *src = tft.getBuffer();
			for (int i = 0; i < 8; i++) {
				// do time-sensitive background tasks
				captureIncomingBytes();
				updateMicrobitDisplay();

				oledCmd(0x10);
				oledCmd(isOLED1106 ? 0x02 : 0); // column offset
				oledCmd(0xB0 + i);

				// write 128 bytes of data in two i2c writes
				memcpy(&buffer[1], src, 64);
				i2cWriteBytes(buffer, 65);
				src += 64;
				memcpy(&buffer[1], src, 64);
				i2cWriteBytes(buffer, 65);
				src += 64;
			}
		}

		#undef UPDATE_DISPLAY
		#define UPDATE_DISPLAY() { if (!deferUpdates) { oledUpdate(); taskSleep(-1); }}
    #elif defined(ARDUINO_IOT_BUS) & defined(TFT_ESPI)
	#define HAS_TOUCH_SCREEN 1
		#include <TFT_eSPI.h>

		// in User_Setup,h #define ILI9341_DRIVER
		// this definition also defines width and height.

		#include <XPT2046_Touchscreen.h>
		TFT_eSPI tft = TFT_eSPI();  // Invoke TFT object
		XPT2046_Touchscreen ts(TOUCH_CS);
		SPIClass& spix = SPI;
//SPIClass mySPI(HSPI); 

		void tftInit() {
			
			tft.init();
			tft.initDMA();
			//tft.setSwapBytes(true);
			spix = tft.getSPIinstance(); 
			//tft.fillScreen(TFT_BLACK);
		
			tft.begin();
			tft.setRotation(3);
	//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			pinMode(2, OUTPUT);
			digitalWrite(2, HIGH);

			useTFT = true;
	}
  
	static void touchInit() {
		
		ts.begin(spix);
		//pinMode(TOUCH_CS, INPUT);
		//mySPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
		//ts.begin(mySPI);
  		ts.setRotation(3);
		//ts.setCalibration(X_MIN, X_MAX, Y_MIN, Y_MAX);
		//ts.setRotation(1);
		touchEnabled = true;
	}

	static int screenTouched() {
		if (!touchEnabled) touchInit();
		// char s[100];
		// sprintf(s,"touch init: %d ",ts.touched());
		// outputString(s);
		return ts.touched();
	}

	static int screenTouchX() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t x = ts.getPoint().x;
		// char s[100];
		// sprintf(s,"touch x: %d ",x);
		// outputString(s);
		return map(x, 200, 3800, TFT_HEIGHT,0);
		}
		
	static int screenTouchY() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		int16_t y = ts.getPoint().y;
		// char s[100];
		// sprintf(s,"touch y: %d ",y);
		// outputString(s);
		return  map(y, 300, 3900, 0, TFT_WIDTH);
		}
		
	static int screenTouchPressure() {
		if (!touchEnabled) touchInit();
		if (!ts.touched()) { return -1; }
		return ts.getPoint().z;
		}
		


	#elif defined(TTGO_DISPLAY)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7789.h"

		#define TFT_MOSI 19
		#define TFT_SCLK 18
		#define TFT_CS 5
		#define TFT_DC 16
		#define TFT_RST 23
		#define TFT_BL 4
		#define TFT_WIDTH 240
		#define TFT_HEIGHT 135
		#define TFT_PWR 22
		Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

		void tftInit() {
			pinMode(TFT_BL, OUTPUT);
			digitalWrite(TFT_BL, 1);
			tft.init(TFT_HEIGHT, TFT_WIDTH);
			tft.setRotation(1);
			tftClear();
			useTFT = true;
		}

	#elif defined(GAMEPAD_DISPLAY)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7735.h"

		#define TFT_MOSI 13
		#define TFT_SCLK 14
		#define TFT_CS 18
		#define TFT_DC 16
		#define TFT_RST 17
		#define TFT_WIDTH 128
		#define TFT_HEIGHT 128
		Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

		void tftInit() {
			tft.initR(INITR_144GREENTAB);
			tft.setRotation(3);
			tft.fillScreen(BLACK);
			useTFT = true;
		}

	#elif defined(TTGO_RP2040)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7789.h"

		#define TFT_MOSI 3
		#define TFT_SCLK 2
		#define TFT_CS 5
		#define TFT_DC 1
		#define TFT_RST 0
		#define TFT_BL 4
		#define TFT_WIDTH 240
		#define TFT_HEIGHT 135
		#define TFT_PWR 22
		Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

		void tftInit() {
			pinMode(TFT_PWR, OUTPUT);
			pinMode(TFT_BL, OUTPUT);
			digitalWrite(TFT_PWR, 1);
			tft.init(TFT_HEIGHT, TFT_WIDTH);
			analogWrite(TFT_BL, 250);
			tft.setRotation(1);
			tftClear();
			useTFT = true;
		}

	#elif defined(PICO_ED)
		#include <Adafruit_GFX.h>

		#define TFT_WIDTH 17
		#define TFT_HEIGHT 7
		#define IS_GRAYSCALE true

		// IS31FL3731 constants
		#define IS31FL_ADDR 0x74
		#define IS31FL_BANK_SELECT 0xFD
		#define IS31FL_FUNCTION_BANK 0x0B
		#define IS31FL_SHUTDOWN_REG 0x0A
		#define IS31FL_CONFIG_REG 0x00
		#define IS31FL_PICTUREFRAME_REG 0x01

		#undef UPDATE_DISPLAY
		#define UPDATE_DISPLAY() { if (!deferUpdates) tft.updateDisplay(); }

		class IS31FL3731 : public Adafruit_GFX {
		public:
			IS31FL3731(uint8_t width, uint8_t height) : Adafruit_GFX(width, height) {}

			TwoWire *wire;
			uint8 displayBuffer[144];

			bool begin();
			void drawPixel(int16_t x, int16_t y, uint16_t brightness);
			void clearDisplayBuffer();
			void showMicroBitPixels(int microBitDisplayBits, int xPos, int yPos);
			void updateDisplay(void);
			void setRegister(uint8_t reg, uint8_t value);
		};

		bool IS31FL3731::begin() {
			wire = &Wire1;
			if (readI2CReg(IS31FL_ADDR, 0) < 0) {
				// no display on external i2c bus, so this is a pico:ed v2

				// initialize internal i2c bus
				wire = &Wire;
				wire->setSDA(0);
				wire->setSCL(1);
				wire->begin();
				wire->setClock(400000);

				// speaker in on pin 3 of pico:ed v2
				setPicoEdSpeakerPin(3);
			}

			// select the function bank
			setRegister(IS31FL_BANK_SELECT, IS31FL_FUNCTION_BANK);

			// toggle shutdown
			setRegister(IS31FL_SHUTDOWN_REG, 0);
			delay(10);
			setRegister(IS31FL_SHUTDOWN_REG, 1);

			// picture mode
			setRegister(IS31FL_CONFIG_REG, 0);

			// set frame to display
			setRegister(IS31FL_PICTUREFRAME_REG, 0);

			// clear the display before enabling LED's
			memset(displayBuffer, 0, sizeof(displayBuffer));
			updateDisplay();

			// enable all LEDs
			for (uint8_t bank = 0; bank < 8; bank++) {
				setRegister(IS31FL_BANK_SELECT, bank);
				for (uint8_t i = 0; i < 18; i++) {
					setRegister(i, 0xFF);
				}
			}
			return true;
		}

		void IS31FL3731::clearDisplayBuffer() {
			memset(displayBuffer, 0, sizeof(displayBuffer));
		}

		void IS31FL3731::drawPixel(int16_t x, int16_t y, uint16_t brightness) {
			// Set the brightness of the pixel at (x, y).

			const uint8_t topRow[17] =
				{7, 23, 39, 55, 71, 87, 103, 119, 135, 136, 120, 104, 88, 72, 56, 40, 24};

			if ((x < 0) || (x > 16)) return;
			if ((y < 0) || (y > 6)) return;

			// adjust brightness (use range 0-100 to avoid making LED's painfully bright)
			if ((brightness != 0) && (brightness < 3)) brightness = 3; //
			brightness = (100 * brightness) / 255;
			if (brightness > 100) brightness = 100;

			int incr = (x < 9) ? -1 : 1;
			int i = topRow[x] + (y * incr);
			displayBuffer[i] = brightness;
		}

		void IS31FL3731::showMicroBitPixels(int microBitDisplayBits, int xPos, int yPos) {
			// Draw 5x5 image at the given location where 1,1 is the origin.

			int brightness = 100;
			int y = yPos;
			for (int i = 0; i < 25; i++) {
				int x = (i % 5) + 5 + xPos;
				if ((5 < x) && (x < 11) && (0 < y) && (y < 6)) {
					if (microBitDisplayBits & (1 << i)) drawPixel(x, y, brightness);
				}
				if ((i % 5) == 4) y++;
			}
			updateDisplay();
		}

		void IS31FL3731::updateDisplay() {
			// Write the entire display buffer to bank 0.

			setRegister(IS31FL_BANK_SELECT, 0); // select bank 0
			for (uint8_t i = 0; i < 6; i++) {
				wire->beginTransmission(IS31FL_ADDR);
				wire->write(0x24 + (24 * i)); // offset within bank
				wire->write(&displayBuffer[24 * i], 24);
				wire->endTransmission();
			}
		}

		void IS31FL3731::setRegister(uint8_t reg, uint8_t value) {
			wire->beginTransmission(IS31FL_ADDR);
			wire->write(reg);
			wire->write(value);
			wire->endTransmission();
		}

		// pretend display is 7 pixels wider so GFX will display partial characters
		IS31FL3731 tft = IS31FL3731(TFT_WIDTH + 7, TFT_HEIGHT);

		void tftInit() {
			tft.begin();
			useTFT = true;
		}

	void showMicroBitPixels(int microBitDisplayBits, int xPos, int yPos) {
		// Used by scrolling text; don't clear display.
		tft.showMicroBitPixels(microBitDisplayBits, xPos, yPos);
	}

	#elif defined(COCUBE)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7789.h"
		#include <LittleFS.h>

		#define TFT_MOSI 19
		#define TFT_SCLK 27
		#define TFT_CS -1
		#define TFT_DC 32
		#define TFT_RST 2
		#define TFT_BL 33
		#define TFT_WIDTH 240
		#define TFT_HEIGHT 240
		#define DEFAULT_BATTERY_PIN 34
		#define LOGO_PATH "/logo.raw"

		SPIClass CoCubeSPI(VSPI);
		Adafruit_ST7789 tft = Adafruit_ST7789(&CoCubeSPI, TFT_CS, TFT_DC, TFT_RST);

		void drawRawImage(const char* filename, int x0, int y0, int width, int height) {
			if (!LittleFS.begin()) return;

			File file = LittleFS.open(filename, "r");
			if (!file) return;

			uint16_t lineBuf[width];
			for (int y = 0; y < height; y++) {
				size_t read = file.read((uint8_t*)lineBuf, width * 2);
				if (read != width * 2) break;
				tft.drawRGBBitmap(x0, y0 + y, lineBuf, width, 1);
			}
			file.close();
		}

		void drawBatteryStatus(int percentage, int x, int y, int width, int height, int textSize) {
			uint16_t fillColor = 0x07E0; // GREEN
			if (percentage < 67) fillColor = 0xFD20; // YELLOW
			if (percentage < 34) fillColor = 0xF800; // RED

			uint16_t borderColor = 0x0000; // BLACK
			uint16_t textColor = 0x0000;   // BLACK by default

			int level = map(percentage, 0, 100, 0, width - 4);
			tft.drawRoundRect(x, y, width, height, 3, borderColor);
			int headW = width / 10;
			tft.fillRect(x + width, y + height / 4, headW, height / 2, 0x4208);
			tft.fillRect(x + 2, y + 2, level, height - 4, fillColor);

			char buf[6];
			sprintf(buf, "%d%%", percentage);

			int charW = 6 * textSize;
			int charH = 8 * textSize;
			int textLen = strlen(buf);

			int textX = x + (width - textLen * charW) / 2;
			int textY = y + (height - charH) / 2;

			tft.setTextColor(textColor);
			tft.setTextSize(textSize);
			tft.setCursor(textX, textY);
			tft.print(buf);
		}

		void tftInit() {
			CoCubeSPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
			tft.init(TFT_HEIGHT, TFT_WIDTH, SPI_MODE3);
			tft.setRotation(1);
			pinMode(TFT_BL, OUTPUT);
			digitalWrite(TFT_BL, HIGH);
			useTFT = true;

			int batteryRaw = 0;
			for (int i = 0; i < 5; i++) {
				batteryRaw += analogRead(DEFAULT_BATTERY_PIN);
				delay(5);
			}
			int battery_percentage = constrain(((44 * batteryRaw / 105.0 - 6800) / 16.0), 0, 99);

			bool logoDisplayed = false;
			LittleFS.begin();
			File logo = LittleFS.open(LOGO_PATH, "r");
			if (logo) {
				logo.close();
				drawRawImage(LOGO_PATH, 0, 0, 240, 240);
				drawBatteryStatus(battery_percentage, 85, 150, 70, 40, 3);
				logoDisplayed = true;
				delay(1000);
			}

			if (!logoDisplayed) {
				tft.fillScreen(WHITE);
				drawBatteryStatus(battery_percentage, 85, 100, 70, 40, 3);
				delay(1000);
			}
			tft.fillScreen(BLACK);
		}

	#elif defined(M5Atom_S3_TFT)
		#undef BLACK // defined GFX
		#undef WHITE // defined GFX
		#define drawRGBBitmap draw16bitRGBBitmap
		#include <Arduino_GFX_Library.h>
		#define TFT_MOSI 21
		#define TFT_SCLK 17
		#define TFT_CS   15
		#define TFT_DC   33
		#define TFT_RST  34
		#define TFT_BL   16
		#define TFT_WIDTH 128
		#define TFT_HEIGHT 128
		Arduino_ESP32SPI bus = Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
		Arduino_GC9107 tft = Arduino_GC9107(&bus, TFT_RST, 0 /* rotation */, true /* IPS */);

		void tftInit() {
			tft.begin();
			tftClear();
			pinMode(TFT_BL, OUTPUT);
			digitalWrite(TFT_BL, HIGH);
			useTFT = true;
		}

#endif // end of board-specific sections

static int hasTFT() {
	#if defined(OLED_128_64)
		if (!useTFT) tftInit();
	#endif
	return useTFT;
}

#define BUFFER_PIXELS_SIZE (TFT_WIDTH * 4)
//sodb move buffer to psram. Warning: No check whether psram exists!
//#if defined(LVGL) && defined(BOARD_HAS_PSRAM  )
//  __attribute__((section(".ext_ram"))) uint16_t bufferPixels[BUFFER_PIXELS_SIZE];
//#elif !defined(CYG)
  uint16_t bufferPixels[BUFFER_PIXELS_SIZE]; // used by primPixelRow and primDrawBuffer
//#endif

static int color24to16b(int color24b) {
	// Convert 24-bit RGB888 format to the TFT's target pixel format.
	// Return [0..1] for 1-bit display, [0-255] for grayscale, and RGB565 for 16-bit color.

	int r, g, b;

	#ifdef IS_MONOCHROME
		return color24b ? 1 : 0;
	#endif

	#ifdef IS_GRAYSCALE
		r = (color24b >> 16) & 0xFF;
		g = (color24b >> 8) & 0xFF;
		b = color24b & 0xFF;
		int gray = r;
		if (g > r) gray = g;
		if (b > r) gray = b;
		return gray;
	#endif

	r = (color24b >> 19) & 0x1F; // 5 bits
	g = (color24b >> 10) & 0x3F; // 6 bits
	b = (color24b >> 3) & 0x1F; // 5 bits
	#if defined(ARDUINO_M5Stick_C) && !defined(ARDUINO_M5Stick_Plus)
		return (b << 11) | (g << 5) | r; // color order: BGR
	#else
		return (r << 11) | (g << 5) | b; // color order: RGB
	#endif
}

void tftClear() {
	if (!hasTFT()) return;

	tft.fillScreen(BLACK);
	UPDATE_DISPLAY();
}

void tftSetHugePixel(int x, int y, int state) {
	if (!useTFT) return;

	#if defined(ARDUINO_BBC_MICROBIT) || defined(ARDUINO_BBC_MICROBIT_V2) || \
		defined(ARDUINO_CALLIOPE_MINI) || defined(CALLIOPE_V3)
			// allow independent use of OLED and micro:bit display
			return;
	#endif

	// simulate a 5x5 array of square pixels like the micro:bit LED array
	#if defined(PICO_ED)
		if ((1 <= x) && (x <= 5) && (1 <= y) && (y <= 5)) {
			int brightness = (state ? 100 : 0);
			tft.drawPixel((x + 5), y, brightness);
			UPDATE_DISPLAY();
		}
		return;
	#endif
	int minDimension, xInset = 0, yInset = 0;
	if (tft.width() > tft.height()) {
		minDimension = tft.height();
		xInset = (tft.width() - tft.height()) / 2;
	} else {
		minDimension = tft.width();
		yInset = (tft.height() - tft.width()) / 2;
	}
	int lineWidth = (minDimension > 60) ? 3 : 1;
	int squareSize = (minDimension - (6 * lineWidth)) / 5;
	tft.fillRect(
		xInset + ((x - 1) * squareSize) + (x * lineWidth), // x
		yInset + ((y - 1) * squareSize) + (y * lineWidth), // y
		squareSize, squareSize,
		color24to16b(state ? mbDisplayColor : BLACK));
	UPDATE_DISPLAY();
}

void tftSetHugePixelBits(int bits) {
	if (!useTFT) return;

	#if defined(ARDUINO_BBC_MICROBIT) || defined(ARDUINO_BBC_MICROBIT_V2)
		// allow independent use TFT and micro:bit display
		return;
	#endif

	#if defined(PICO_ED)
		tft.clearDisplayBuffer();
		tft.showMicroBitPixels(bits, 1, 1);
		return;
	#endif
	if (0 == bits) {
		tftClear();
	} else {
		deferUpdates = true;
		for (int x = 1; x <= 5; x++) {
			for (int y = 1; y <= 5; y++) {
				tftSetHugePixel(x, y, bits & (1 << ((5 * (y - 1) + x) - 1)));
			}
		}
		deferUpdates = false;
	}
	UPDATE_DISPLAY();
}

OBJ primSetBacklight(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	if ((argCount < 1) || !isInt(args[0])) return falseObj;
	int brightness = obj2int(args[0]);
	(void) (brightness); // reference var to suppress compiler warning

	#if defined(ARDUINO_IOT_BUS)
		pinMode(33, OUTPUT);
		digitalWrite(33, (brightness > 0) ? HIGH : LOW);
	#elif defined(LMS7789)
		pinMode(12, OUTPUT);
		analogWrite(12, brightness * 25);
	#elif defined(LMSDISPLAY)
		pinMode(33, OUTPUT);
		analogWrite(33, brightness * 25);
	#elif defined(COCUBE)
		pinMode(TFT_BL, OUTPUT);
		if (brightness < 0) brightness = 0;
		if (brightness > 10) brightness = 10;
		analogWrite(TFT_BL, brightness * 25);
	#elif defined(ARDUINO_M5Stack_Core_ESP32)
		pinMode(32, OUTPUT);
		digitalWrite(32, (brightness > 0) ? HIGH : LOW);
	#elif defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_Plus)
		brightness = (brightness <= 0) ? 0 : brightness + 7; // 8 is lowest setting that turns on backlight
		if (brightness > 15) brightness = 15;
		int n = readAXP(0x28);
		writeAXP(0x28, (brightness << 4) | (n & 0x0f)); // set brightness (high 4 bits of reg 0x28)
	#elif defined(ARDUINO_NRF52840_CLUE)
		pinMode(34, OUTPUT);
		digitalWrite(34, (brightness > 0) ? HIGH : LOW);
	#elif defined(TTGO_RP2040)
		pinMode(TFT_BL, OUTPUT);
		if (brightness < 0) brightness = 0;
		if (brightness > 10) brightness = 10;
		analogWrite(TFT_BL, brightness * 25);
	#elif defined(OLED_128_64)
		int oledLevel = (255 * brightness) / 10;
		if (oledLevel < 0) oledLevel = 0;
		if (oledLevel > 255) oledLevel = 255;
		writeI2CReg(TFT_ADDR, 0x80, 0x81);
		writeI2CReg(TFT_ADDR, 0x80, oledLevel);
	#endif
	return falseObj;
}

static OBJ primGetWidth(int argCount, OBJ *args) {
	if (!hasTFT()) return zeroObj;

	#ifdef TFT_WIDTH
		#if defined(LMSDISPLAY)
			return int2obj(TFT_HEIGHT);
		#else
			return int2obj(TFT_WIDTH);
		#endif
	#else
		return int2obj(0);
	#endif
}

static OBJ primGetHeight(int argCount, OBJ *args) {
	if (!hasTFT()) return zeroObj;

	#ifdef TFT_HEIGHT
		#if defined(LMSDISPLAY)
			return int2obj(TFT_WIDTH);
		#else
			return int2obj(TFT_HEIGHT);
		#endif
	#else
		return int2obj(0);
	#endif
}

static OBJ primSetPixel(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	int color16b = color24to16b(obj2int(args[2]));
	tft.drawPixel(x, y, color16b);
	UPDATE_DISPLAY();
	return falseObj;
}


//#if !defined(CYD)
static OBJ primPixelRow(int argCount, OBJ *args) {
	// Draw a single row of pixels (a list or byte array) at the given y.
	// If a byte array is provided the optional argument bytesPerPixel
	// determines the pixel size: 2, 3 or 4 bytes.
	// 2 means 16-bit RGB565 pixels; -2 means 16-bit RGB555 pixels.
	// 32 and 24 bit pixels are RGB(A) byte order. (Alpha of 32-bit pixels is ignored).
	// Used to accelerate BMP file display and other bitmap operations.

	if (!hasTFT()) return falseObj;
	#if defined(LMSDISPLAY)
		#define _TFT_WIDTH TFT_HEIGHT
		#define _TFT_HEIGHT TFT_WIDTH
	#elif defined(CYDROT)
		#define _TFT_WIDTH TFT_HEIGHT
		#define _TFT_HEIGHT TFT_WIDTH
	#else
		#define _TFT_WIDTH TFT_WIDTH
		#define _TFT_HEIGHT TFT_HEIGHT
	#endif
	OBJ pixelDataObj = args[0];
	int x = obj2int(args[1]);
	if (x >= _TFT_WIDTH) return falseObj;
	int y = obj2int(args[2]);
	if ((y < 0) || (y >= _TFT_HEIGHT)) return falseObj;
	int bytesPerPixel = ((argCount > 3) && isInt(args[3])) ? obj2int(args[3]) : 4;

	uint32 palette[256];
	if ((argCount > 4) && IS_TYPE(args[4], ListType)) {
		// paletteObj is a list of Integers representingRGB colors
		// palette is a C array of TFT display pixel values (e.g. 16-bit colors)
		OBJ paletteObj = args[4];
		int colorCount = obj2int(FIELD(paletteObj, 0)); // list size
		if (colorCount > 256) colorCount = 256;
		memset(palette, 0, sizeof(palette));
		for (int i = 0; i < colorCount; i++) {
			int rgb = obj2int(FIELD(paletteObj, i + 1));
			palette[i] = color24to16b(rgb & 0xFFFFFF);
		}
	}

	if (IS_TYPE(pixelDataObj, ListType)) {
		int pixelCount = obj2int(FIELD(pixelDataObj, 0));
		if (pixelCount > (_TFT_WIDTH - x)) pixelCount = _TFT_WIDTH - x;
		if (pixelCount > BUFFER_PIXELS_SIZE) pixelCount = BUFFER_PIXELS_SIZE;
		for (int i = 0; i < pixelCount; i++) {
			OBJ pixelObj = FIELD(pixelDataObj, (i + 1));
			bufferPixels[i] = (isInt(pixelObj)) ? color24to16b(obj2int(pixelObj)) : 0;
		}
		//sodb
		 #if defined(TFT_ESPI)
		   //tft.pushImageDMA(x,y,pixelCount,1,bufferPixels);
		   // no DMA possible from PSRAM
		   tft.pushImage(x,y,pixelCount,1,bufferPixels);
		  #elif defined(CYDS343)
		  	// no tft
			tft.draw16bitRGBBitmap(x, y, bufferPixels, pixelCount, 1);
			// in block with w and h
			// now just do nothing
		   #else
		  tft.drawRGBBitmap(x, y, bufferPixels, pixelCount, 1);
		#endif
		
	} else if (IS_TYPE(pixelDataObj, ByteArrayType)) {
		int isRGB565 = true;
		if (bytesPerPixel < 0) {
			isRGB565 = false; // -2 means 16-bit RGB555 (vs. RGB565)
			bytesPerPixel = -bytesPerPixel;
		}
		if ((bytesPerPixel < 1) || (bytesPerPixel > 4)) return falseObj;

		int pixelCount = BYTES(pixelDataObj) / bytesPerPixel;
		if (pixelCount > (_TFT_WIDTH - x)) pixelCount = _TFT_WIDTH - x;
		if (pixelCount > BUFFER_PIXELS_SIZE) pixelCount = BUFFER_PIXELS_SIZE;
		uint8 *byte = (uint8 *) &FIELD(pixelDataObj, 0);
		if (1 == bytesPerPixel) {
			for (int i = 0; i < pixelCount; i++) {
				bufferPixels[i] = palette[*byte++];
			}
		} else if (2 == bytesPerPixel) {
			for (int i = 0; i < pixelCount; i++) {
				int pixel = (byte[1] << 8) | byte[0];
				int r = isRGB565 ? ((pixel >> 8) & 248) : ((pixel >> 7) & 248);
				int g = isRGB565 ? ((pixel >> 3) & 252) : ((pixel >> 2) & 248);
				int b = (pixel << 3) & 248;
				bufferPixels[i] = color24to16b((r << 16) | (g << 8) | b);
				byte += bytesPerPixel;
			}
		} else { // 24-bit or 32-bit pixels
			for (int i = 0; i < pixelCount; i++) {
				bufferPixels[i] = color24to16b((byte[2] << 16) | (byte[1] << 8) | byte[0]);
				byte += bytesPerPixel;
			}
		}
		//sodb
		// tft.drawRGBBitmap(x, y, bufferPixels, pixelCount, 1);
		//tft.draw16bitRGBBitmap(x, y, bufferPixels, pixelCount, 1);
		#if defined(TFT_ESPI)
			//tft.pushImageDMA(x, y, pixelCount, 1, bufferPixels);
			tft.pushImage(x, y, pixelCount, 1, bufferPixels);
		#elif defined(CYDS343)
		  	// no tft
			tft.draw16bitRGBBitmap(x, y, bufferPixels, pixelCount, 1);
			// in block with w and h
			// now just do nothing
		#else
		  tft.drawRGBBitmap(x, y, bufferPixels, pixelCount, 1);
		#endif
	}
	
	UPDATE_DISPLAY();
	return falseObj;
}

//#endif 

static OBJ primLine(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	int x0 = obj2int(args[0]);
	int y0 = obj2int(args[1]);
	int x1 = obj2int(args[2]);
	int y1 = obj2int(args[3]);
	int color16b = color24to16b(obj2int(args[4]));
	tft.drawLine(x0, y0, x1, y1, color16b);
	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primRect(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	int width = obj2int(args[2]);
	int height = obj2int(args[3]);
	int color16b = color24to16b(obj2int(args[4]));
	int fill = (argCount > 5) ? (trueObj == args[5]) : true;
	if (fill) {
		tft.fillRect(x, y, width, height, color16b);
	} else {
		tft.drawRect(x, y, width, height, color16b);
	}
	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primRoundedRect(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	int width = obj2int(args[2]);
	int height = obj2int(args[3]);
	int radius = obj2int(args[4]);
	int color16b = color24to16b(obj2int(args[5]));
	int fill = (argCount > 6) ? (trueObj == args[6]) : true;
	if (fill) {
		tft.fillRoundRect(x, y, width, height, radius, color16b);
	} else {
		tft.drawRoundRect(x, y, width, height, radius, color16b);
	}
	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primCircle(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	int radius = obj2int(args[2]);
	int color16b = color24to16b(obj2int(args[3]));
	int fill = (argCount > 4) ? (trueObj == args[4]) : true;
	if (fill) {
		tft.fillCircle(x, y, radius, color16b);
	} else {
		tft.drawCircle(x, y, radius, color16b);
	}
	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primTriangle(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	int x0 = obj2int(args[0]);
	int y0 = obj2int(args[1]);
	int x1 = obj2int(args[2]);
	int y1 = obj2int(args[3]);
	int x2 = obj2int(args[4]);
	int y2 = obj2int(args[5]);
	int color16b = color24to16b(obj2int(args[6]));
	int fill = (argCount > 7) ? (trueObj == args[7]) : true;
	if (fill) {
		tft.fillTriangle(x0, y0, x1, y1, x2, y2, color16b);
	} else {
		tft.drawTriangle(x0, y0, x1, y1, x2, y2, color16b);
	}
	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primText(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;

	OBJ value = args[0];
	int x = obj2int(args[1]);
	int y = obj2int(args[2]);
	int color16b = color24to16b(obj2int(args[3]));
	int scale = (argCount > 4) ? obj2int(args[4]) : 2;
	int wrap = (argCount > 5) ? (trueObj == args[5]) : true;
	int bgColor = (argCount > 6) ? color24to16b(obj2int(args[6])) : -1;
	tft.setCursor(x, y);
	tft.setTextColor(color16b);
	tft.setTextSize(scale);
	tft.setTextWrap(wrap);

	int lineH = 8 * scale;
	int letterW = 6 * scale;
	if (IS_TYPE(value, StringType)) {
	char *str = obj2str(value);
	if (bgColor != -1) tft.fillRect(x, y, strlen(str) * letterW, lineH, bgColor);
		tft.print(obj2str(value));
	} else if (trueObj == value) {
		if (bgColor != -1) tft.fillRect(x, y, 4 * letterW, lineH, bgColor);
		tft.print("true");
	} else if (falseObj == value) {
		if (bgColor != -1) tft.fillRect(x, y, 5 * letterW, lineH, bgColor);
		tft.print("false");
	} else if (isInt(value)) {
		char s[50];
		sprintf(s, "%d", obj2int(value));
		if (bgColor != -1) tft.fillRect(x, y, strlen(s) * letterW, lineH, bgColor);
		tft.print(s);
	}
	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primClear(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;
	tftClear();
	return falseObj;
}

// Aruco and April tags

const uint16_t aruco_tags[100] = {
	0X4ACD, 0XF065, 0XCCD2, 0X66B9, 0XAB61, 0X8632, 0X61D1, 0X3B0D, 0X0125, 0X30A9, 0X066E, 0XEE58, 0XF148,
	0XD5F0, 0XDB4E, 0XD9C1, 0XB99A, 0X99FF, 0X93A1, 0X8950, 0X7974, 0X4FD4, 0X332A, 0X227D, 0X01B8, 0X6B8E,
	0X531B, 0X5AAB, 0XDEDC, 0XCB90, 0XBBEA, 0XA84D, 0X6130, 0X0F34, 0XF751, 0XF6D6, 0XE78A, 0XFB00, 0XF209,
	0XE3A5, 0XE8E7, 0XD5D7, 0XCD73, 0XC74D, 0XDB17, 0XD114, 0XD2C0, 0XB49B, 0XAFD1, 0XAFEC, 0XAE6B, 0XAA97,
	0XA2BE, 0XA068, 0X97FE, 0X9798, 0XEDB,  0X9E16, 0X94ED, 0X901A, 0X9820, 0X81E4, 0X7F5F, 0X7CBB, 0X745D,
	0X6C85, 0X7B93, 0X7AD5, 0X7A63, 0X6376, 0X605E, 0X4483, 0X43FB, 0X49A4, 0X4037, 0X4854, 0X35E0, 0X369D,
	0X26A7, 0X2C2A, 0X3367, 0X385F, 0X3AC8, 0X16A2, 0X06DA, 0X0444, 0X11D5, 0X08B2, 0XCA8A, 0X7552, 0X89E8,
	0XF530, 0XF9B4, 0XD23E, 0XB627, 0XBC0B, 0XB0C9, 0XB02C, 0X961B, 0X8F38};

const uint64_t april_tags[100] = {
	0x0004064a19651ff1, 0x0004064a53f425b6, 0x0004064a8e832b7b, 0x0004064ac9123140, 0x0004064b03a13705,
	0x0004064b3e303cca, 0x0004064b78bf428f, 0x0004064bb34e4854, 0x0004064beddd4e19, 0x0004064c286c53de,
	0x0004064c62fb59a3, 0x0004064c9d8a5f68, 0x0004064d12a86af2, 0x0004064d4d3770b7, 0x0004064dc2557c41,
	0x0004064dfce48206, 0x0004064e377387cb, 0x0004064e72028d90, 0x0004064eac919355, 0x0004064f21af9edf,
	0x0004064fd15cb02e, 0x000406500bebb5f3, 0x00040650467abbb8, 0x00040650bb98c742, 0x00040650f627cd07,
	0x000406516b45d891, 0x00040651a5d4de56, 0x000406521af2e9e0, 0x000406525581efa5, 0x00040653052f00f4,
	0x000406533fbe06b9, 0x000406537a4d0c7e, 0x00040653ef6b1808, 0x0004065429fa1dcd, 0x0004065464892392,
	0x000406549f182957, 0x00040654d9a72f1c, 0x00040655143634e1, 0x000406554ec53aa6, 0x000406558954406b,
	0x00040655c3e34630, 0x00040655fe724bf5, 0x000406567390577f, 0x00040656ae1f5d44, 0x00040657233d68ce,
	0x00040657985b7458, 0x00040657d2ea7a1d, 0x00040658480885a7, 0x00040658bd269131, 0x00040659e1f1ae0a,
	0x0004065a919ebf59, 0x0004065bb669dc32, 0x0004065bf0f8e1f7, 0x0004065cdb34f90b, 0x0004065d15c3fed0,
	0x0004065d50530495, 0x0004065e3a8f1ba9, 0x0004065eea3c2cf8, 0x0004066049964f96, 0x000406608425555b,
	0x00040660beb45b20, 0x0004066133d266aa, 0x00040661e37f77f9, 0x000406621e0e7dbe, 0x00040662932c8948,
	0x00040662cdbb8f0d, 0x00040663084a94d2, 0x0004066342d99a97, 0x000406637d68a05c, 0x00040663f286abe6,
	0x0004066467a4b770, 0x00040664a233bd35, 0x00040664dcc2c2fa, 0x000406651751c8bf, 0x0004066551e0ce84,
	0x00040666b13af122, 0x00040666ebc9f6e7, 0x0004066760e80271, 0x00040668109513c0, 0x000406684b241985,
	0x00040668fad12ad4, 0x000406696fef365e, 0x00040669aa7e3c23, 0x00040669e50d41e8, 0x0004066bb9857010,
	0x0004066bf41475d5, 0x0004066c6932815f, 0x0004066ca3c18724, 0x0004066d536e9873, 0x0004066dc88ca3fd,
	0x0004066e031ba9c2, 0x0004066eb2c8bb11, 0x000406704cb1e374, 0x00040670c1cfeefe, 0x00040670fc5ef4c3,
	0x0004067136edfa88, 0x00040671ac0c0612, 0x00040673bb1339ff, 0x000406746ac04b4e, 0x00040676b4568500};

const int april_bit_x[52] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 3, 4, 5, 4, 9, 9, 9, 9, 9, 9, 9, 9, 9, 6, 6, 6, 5,
	9, 8, 7, 6, 5, 4, 3, 2, 1, 6, 5, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 4};
const int april_bit_y[52] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 3, 4, 5, 4,
	9, 9, 9, 9, 9, 9, 9, 9, 9, 6, 6, 6, 5, 9, 8, 7, 6, 5, 4, 3, 2, 1, 6, 5, 4, 5};

static OBJ primAruco(int argCount, OBJ *args) {
	if (!useTFT) return falseObj;
	#if defined(LMSDISPLAY)
		#define _TFT_WIDTH TFT_HEIGHT
		#define _TFT_HEIGHT TFT_WIDTH
	#else
		#define _TFT_WIDTH TFT_WIDTH
		#define _TFT_HEIGHT TFT_HEIGHT
	#endif
	int aruco_id = evalInt(args[0]);
	if (aruco_id >= 100) {
		return falseObj;
	}
	tft.drawRect(0, 0, _TFT_HEIGHT, _TFT_HEIGHT, BLACK);
	const int cellSize = _TFT_HEIGHT/8;
	const int startX = _TFT_WIDTH/2 - (4 * cellSize);
	uint16_t tag = aruco_tags[aruco_id];
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			bool isBlack = false;

			if (i == 0 || i == 7 || j == 0 || j == 7) {
				// 外层白色边框 (outer white border)
				isBlack = false;
			} else if (i == 1 || i == 6 || j == 1 || j == 6) {
				// 内层黑色边框 (inner black border)
				isBlack = true;
			} else {
				// 中央的4x4区域，用于编码信息 (central 4x4 area for encoding information)
				int bitIndex = (i - 2) * 4 + (j - 2);
				isBlack = tag & (1 << (15 - bitIndex));
			}
			if (isBlack) {
				tft.fillRect(startX + j * cellSize, i * cellSize, cellSize, cellSize, BLACK);
			} else {
				tft.fillRect(startX + j * cellSize, i * cellSize, cellSize, cellSize, WHITE);
			}
		}
	}
	tft.setCursor(startX + 2, 2);
	tft.setTextColor(BLACK);
	tft.setTextSize(2);
	tft.print(aruco_id);
	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primAprilTag(int argCount, OBJ *args) {
	if (!useTFT) return falseObj;

	int tag_id = evalInt(args[0]);
	if (tag_id >= 100) {
		return falseObj;
	}
	#if defined(LMSDISPLAY)
		#define _TFT_WIDTH TFT_HEIGHT
		#define _TFT_HEIGHT TFT_WIDTH
	#else
		#define _TFT_WIDTH TFT_WIDTH
		#define _TFT_HEIGHT TFT_HEIGHT
	#endif
	tft.drawRect(0, 0, _TFT_HEIGHT, _TFT_HEIGHT, BLACK);
	const int cellSize = _TFT_HEIGHT/10;
	const int startX = _TFT_WIDTH/2 - (5 * cellSize);
	uint64_t codedata = april_tags[tag_id];

	// 绘制外圈的黑色方块 (draw outer black square)
	for (int i = 1; i < 9; i++) {
		tft.fillRect(startX + i * cellSize, 1 * cellSize, cellSize, cellSize, BLACK); // 左边界 (left)
		tft.fillRect(startX + i * cellSize, 8 * cellSize, cellSize, cellSize, BLACK); // 右边界 (right)
		tft.fillRect(startX + 1 * cellSize, i * cellSize, cellSize, cellSize, BLACK); // 上边界 (top)
		tft.fillRect(startX + 8 * cellSize, i * cellSize, cellSize, cellSize, BLACK); // 下边界 (bottom)
	}

	// 绘制内圈的白色方块 (draw inner ring of white squares)
	for (int i = 2; i < 8; i++) {
		tft.fillRect(startX + i * cellSize, 2 * cellSize, cellSize, cellSize, WHITE); // 左边界 (left)
		tft.fillRect(startX + i * cellSize, 7 * cellSize, cellSize, cellSize, WHITE); // 右边界 (right)
		tft.fillRect(startX + 2 * cellSize, i * cellSize, cellSize, cellSize, WHITE); // 上边界 (top)
		tft.fillRect(startX + 7 * cellSize, i * cellSize, cellSize, cellSize, WHITE); // 下边界 (bottom)
	}

	// 绘制编码的标签图像 (draw encoded label)
	for (int i = 0; i < 52; i++) {
		int x = april_bit_x[i];
		int y = april_bit_y[i];
		bool bit = (codedata >> (51 - i)) & 1;
		uint16_t color = bit ? WHITE : BLACK;
		tft.fillRect(startX + x * cellSize, y * cellSize, cellSize, cellSize, color);
	}
	UPDATE_DISPLAY();
	return falseObj;
}

// display update control

OBJ primDeferUpdates(int argCount, OBJ *args) {
	if (!useTFT) return falseObj;
	deferUpdates = true;
	return falseObj;
}

OBJ primResumeUpdates(int argCount, OBJ *args) {
	if (!useTFT) return falseObj;
	deferUpdates = false;
	UPDATE_DISPLAY();
	return falseObj;
}

// 8 bit bitmap ops

static OBJ primMergeBitmap(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;
	#if defined(LMSDISPLAY)
		#define _TFT_WIDTH TFT_HEIGHT
		#define _TFT_HEIGHT TFT_WIDTH
	#else
		#define _TFT_WIDTH TFT_WIDTH
		#define _TFT_HEIGHT TFT_HEIGHT
	#endif
	OBJ bitmap = args[0];
	int bitmapWidth = obj2int(args[1]);
	OBJ buffer = args[2];
	int scale = max(min(obj2int(args[3]), 8), 1);
	int alphaIndex = obj2int(args[4]);
	int destX = obj2int(args[5]);
	int destY = obj2int(args[6]);

	int bitmapHeight = BYTES(bitmap) / bitmapWidth;
	int bufferWidth = TFT_WIDTH / scale;
	int bufferHeight = TFT_HEIGHT / scale;
	uint8 *bitmapBytes = (uint8 *) &FIELD(bitmap, 0);
	uint8 *bufferBytes = (uint8 *) &FIELD(buffer, 0);

	for (int y = 0; y < bitmapHeight; y++) {
		if ((y + destY) < bufferHeight && (y + destY) >= 0) {
			for (int x = 0; x < bitmapWidth; x++) {
				if ((x + destX) < bufferWidth && (x + destX) >= 0) {
					int pixelValue = bitmapBytes[y * bitmapWidth + x];
					if (pixelValue != alphaIndex) {
						int bufIndex = (destY + y) * bufferWidth + x + destX;
						bufferBytes[bufIndex] = pixelValue;
					}
				}
			}
		}
	}
	return falseObj;
}

static OBJ primDrawBuffer(int argCount, OBJ *args) {
	if (!hasTFT()) return falseObj;
	#if defined(LMSDISPLAY)
		#define _TFT_WIDTH TFT_HEIGHT
		#define _TFT_HEIGHT TFT_WIDTH
	#else
		#define _TFT_WIDTH TFT_WIDTH
		#define _TFT_HEIGHT TFT_HEIGHT
	#endif
	OBJ buffer = args[0];
	OBJ palette = args[1]; // List, index-1 based
	int scale = max(min(obj2int(args[2]), 8), 1);

	int originX = 0;
	int originY = 0;
	int copyWidth = -1;
	int copyHeight = -1;

	if (argCount > 6) {
		originX = obj2int(args[3]);
		originY = obj2int(args[4]);
		copyWidth = obj2int(args[5]);
		copyHeight = obj2int(args[6]);
	}

	int bufferWidth = _TFT_WIDTH / scale;
	int bufferHeight = _TFT_HEIGHT / scale;

	int originWidth = copyWidth >= 0 ? copyWidth : bufferWidth;
	int originHeight = copyHeight >= 0 ? copyHeight : bufferHeight;

	uint8 *bufferBytes = (uint8 *) &FIELD(buffer, 0);
	// Read the indices from the buffer and turn them into color values from the
	// palette, and paint them onto the TFT
	for (int y = 0; y < originHeight; y ++) {
		for (int x = 0; x < originWidth; x ++) {
			int colorIndex = bufferBytes[
				(y + originY) * bufferWidth + (x + originX)];
			int color = color24to16b(obj2int(FIELD(palette, colorIndex + 1)));
			for (int i = 0; i < scale; i ++) {
				for (int j = 0; j < scale; j ++) {
					bufferPixels[(j * originWidth * scale) + x * scale + i] = color;
				}
			}
		}
		//sodb
		// tft.draw16bitRGBBitmap(
		// 	originX * scale,
		// 	(originY + y) * scale,
		// 	bufferPixels,
		// 	originWidth * scale,
		// 	scale
		// );
		#if defined(TFT_ESPI)
		//tft.pushImageDMA(
		tft.pushImage(
			originX * scale,
			(originY + y) * scale,
			originWidth * scale,
			scale,
			bufferPixels
		);
		#elif defined(CYDS343)
		  	// no tft
			tft.draw16bitRGBBitmap(
			originX * scale,
			(originY + y) * scale,
			bufferPixels,
			originWidth * scale,
			scale
		);
			// in block with w and h
			// now just do nothing
		#else
		tft.drawRGBBitmap(
			originX * scale,
			(originY + y) * scale,
			bufferPixels,
			originWidth * scale,
			scale
		);
		#endif



	}

	UPDATE_DISPLAY();
	return falseObj;
}

static OBJ primDrawBitmap(int argCount, OBJ *args) {
	// Draw an 8-bit bitmap at a given position without scaling.

	if (!hasTFT()) return falseObj;
	uint32 palette[256];
	#if defined(LMSDISPLAY)
		#define _TFT_WIDTH TFT_HEIGHT
		#define _TFT_HEIGHT TFT_WIDTH
	#else
		#define _TFT_WIDTH TFT_WIDTH
		#define _TFT_HEIGHT TFT_HEIGHT
	#endif
	if (argCount < 4) return fail(notEnoughArguments);
	OBJ bitmapObj = args[0]; // bitmap: a two-item list of [width (int), pixels (byte array)]
	OBJ paletteObj = args[1]; // palette: a list of RGB values
	int dstX = obj2int(args[2]);
	int dstY = obj2int(args[3]);

	if ((dstX > _TFT_WIDTH) || (dstY > _TFT_HEIGHT)) return falseObj; // off screen

	// process bitmap arg
	if (!IS_TYPE(bitmapObj, ListType) ||
	 	(obj2int(FIELD(bitmapObj, 0)) != 2) ||
	 	!isInt(FIELD(bitmapObj, 1)) ||
	 	!IS_TYPE(FIELD(bitmapObj, 2), ByteArrayType)) {
	 		return fail(bad8BitBitmap);
	}
	int bitmapWidth = obj2int(FIELD(bitmapObj, 1));
	OBJ bitmapBytesObj = FIELD(bitmapObj, 2);
	int bitmapByteCount = BYTES(bitmapBytesObj);
	if ((bitmapWidth <= 0) || ((bitmapByteCount % bitmapWidth) != 0)) return fail(bad8BitBitmap);
	int bitmapHeight = bitmapByteCount / bitmapWidth;

	// process palette arg
	if (!IS_TYPE(paletteObj, ListType)) return fail(badColorPalette);
	int colorCount = obj2int(FIELD(paletteObj, 0)); // list size
	if (colorCount > 256) colorCount = 256;
	memset(palette, 0, sizeof(palette)); // initialize to all black RGB values
	for (int i = 0; i < colorCount; i++) {
		int rgb = obj2int(FIELD(paletteObj, i + 1));
		if (rgb < 0) rgb = 0;
		if (rgb > 0xFFFFFF) rgb = 0xFFFFFF;
		palette[i] = rgb;
	}

	int srcX = 0;
	int srcW = bitmapWidth;
	if (dstX < 0) { srcX = -dstX; dstX = 0; srcW -= srcX; }
	if (srcW < 0) return falseObj; // off screen to left
	if ((dstX + srcW) > _TFT_WIDTH) srcW = _TFT_WIDTH - dstX;

	int srcY = 0;
	int srcH = bitmapHeight;
	if (dstY < 0) { srcY = -dstY; dstY = 0; srcH -= srcY; }
	if (srcH < 0) return falseObj; // off screen above
	if ((dstY + srcH) > TFT_HEIGHT) srcH = TFT_HEIGHT - dstY;

	uint8 *bitmapBytes = (uint8 *) &FIELD(bitmapBytesObj, 0);
	for (int i = 0; i < srcH; i++) {
		uint8 *row = bitmapBytes + ((srcY + i) * bitmapWidth);
		for (int j = 0; j < srcW; j++) {
			uint8 pix = row[srcX + j]; // 8-bit color index
			uint32 rgb = palette[pix]; // 24 bit RGB color
			tft.drawPixel(dstX + j, dstY + i, color24to16b(rgb));
		}
	}
	UPDATE_DISPLAY();
	return falseObj;
}


#else // stubs

void tftInit() { }
void tftClear() { }
void tftSetHugePixel(int x, int y, int state) { }
void tftSetHugePixelBits(int bits) { }

static OBJ primSetBacklight(int argCount, OBJ *args) { return falseObj; }
static OBJ primGetWidth(int argCount, OBJ *args) { return int2obj(0); }
static OBJ primGetHeight(int argCount, OBJ *args) { return int2obj(0); }
static OBJ primSetPixel(int argCount, OBJ *args) { return falseObj; }
//#if defined(BOARD_HAS_PSRAM)
static OBJ primPixelRow(int argCount, OBJ *args) { return falseObj; }
//#endif
static OBJ primLine(int argCount, OBJ *args) { return falseObj; }
static OBJ primRect(int argCount, OBJ *args) { return falseObj; }
static OBJ primRoundedRect(int argCount, OBJ *args) { return falseObj; }
static OBJ primCircle(int argCount, OBJ *args) { return falseObj; }
static OBJ primTriangle(int argCount, OBJ *args) { return falseObj; }
static OBJ primText(int argCount, OBJ *args) { return falseObj; }
static OBJ primClear(int argCount, OBJ *args) { return falseObj; }

OBJ primDeferUpdates(int argCount, OBJ *args) { return falseObj; }
OBJ primResumeUpdates(int argCount, OBJ *args) { return falseObj; }

static OBJ primMergeBitmap(int argCount, OBJ *args) { return falseObj; }
static OBJ primDrawBuffer(int argCount, OBJ *args) { return falseObj; }
static OBJ primDrawBitmap(int argCount, OBJ *args) { return falseObj; }

static OBJ primAruco(int argCount, OBJ *args) { return falseObj; }
static OBJ primAprilTag(int argCount, OBJ *args) { return falseObj; }

#endif

// LVGL 


#if defined(LVGL) && defined(CYDS343)
#include <lvgl.h>
extern bool useLVGL;
extern bool LVGL_initialized;
void setup_lvgl(void); 

static uint32_t screenWidth;
static uint32_t screenHeight;



void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
#ifndef DIRECT_RENDER_MODE
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  tft.draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  
#endif // #ifndef DIRECT_RENDER_MODE

  /*Call it to tell LVGL you are ready*/
  lv_disp_flush_ready(disp);
}

#if defined(BBCT)

void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {

  	TOUCHINFO ti;
  	if (!touchEnabled) touchInit();
	if (bbct.getSamples(&ti)) {
		for (int i = 0; i < ti.count; i++) {
		int32_t x =ti.x[0];
		int32_t y =ti.y[0];
		//if (x >= 0 && x < 320 && y >= 0 && y < 480) {
			data->point.x = x;
			data->point.y = y;
			// Serial.printf("x,y = %d, %d\n", x, y);
			data->state = LV_INDEV_STATE_PRESSED;
		//}
		}
	} else {
		data->state = LV_INDEV_STATE_RELEASED;  // No touch detected
	}
}


#else

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
 
	/*
	if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PRESSED;

     
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
	//   char s[100];
	//   sprintf(s,"touch %d, %d",touch_last_x, touch_last_y);
    //   outputString(s);
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
	*/

			/*
			uint16_t x_raw,y_raw;
			if (ts.touched()) {
			TS_Point p = ts.getPoint();
			// Map raw touch to screen coordinates
			data->state = LV_INDEV_STATE_PR;
			data->point.x = map(p.x, 200, 3800, 0, TFT_WIDTH);
			data->point.y = map(p.y, 250, 3900,  TFT_HEIGHT,0);
			*/
			// use global functions
			if (screenTouched()) {
				data->state = LV_INDEV_STATE_PRESSED;
				data->point.x = screenTouchX();
				data->point.y = screenTouchY();
			
			} else {
			data->state = LV_INDEV_STATE_RELEASED;
			}
		
}

#endif



#endif


// only for these boards:
#if (defined(LVGL) || defined(CDYR) || defined(CYDC) || defined(S3_CTF))&& defined(TFT_ESPI)
	#include <lvgl.h>
	void setup_lvgl(void); 
	extern bool useLVGL;
	extern bool LVGL_initialized;


	void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
		uint16_t w = area->x2 - area->x1 + 1;
		uint16_t h = area->y2 - area->y1 + 1;

		uint16_t *color_buf = (uint16_t *)px_map;

		tft.startWrite();
		tft.setAddrWindow(area->x1, area->y1, w, h);

		// Send entire area with DMA
		#if defined(S3_CTF)
		  tft.pushPixels(color_buf, w * h);      // ← NOT DMA
		  tft.endWrite();
		  
		#else

		   tft.pushPixelsDMA(color_buf, w * h);

		   tft.endWrite();
		   tft.dmaWait();
		#endif
		// Immediately notify LVGL since TFT_eSPI handles DMA behind the scenes
		lv_disp_flush_ready(disp);
		yield(); // sodb give wifi some air to breath
	}
	/*Read the touchpad*/
	#if defined(HAS_TOUCH_SCREEN)
		void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {
			/*
			uint16_t x_raw,y_raw;
			if (ts.touched()) {
			TS_Point p = ts.getPoint();
			// Map raw touch to screen coordinates
			data->state = LV_INDEV_STATE_PR;
			data->point.x = map(p.x, 200, 3800, 0, TFT_WIDTH);
			data->point.y = map(p.y, 250, 3900,  TFT_HEIGHT,0);
			*/
			// use global functions
			if (!tft.dmaBusy() && screenTouched()) {
				data->state = LV_INDEV_STATE_PR;
				data->point.x = screenTouchX();
				data->point.y = screenTouchY();
			
			} else {
			data->state = LV_INDEV_STATE_RELEASED;
			}
		}
	#endif

#elif defined(LVGL) && !defined(TFT_ESPI)  && !defined(CYDS343)
#include <lvgl.h>
void setup_lvgl(void); 
extern bool useLVGL;
extern bool LVGL_initialized;


void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map) {
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    uint16_t *color_buf = (uint16_t *)px_map;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);

    // Push one row at a time (slightly better for memory caching)
    for (uint32_t i = 0; i < w * h; i++) {
        tft.pushColor(color_buf[i]);
    }

    tft.endWrite();
    lv_disp_flush_ready(disp);
	yield(); // sodb give wifi some air to breath
}
/*Read the touchpad*/

#if defined(COCUBE)
#define BTN_A 38  // GPIO for NEXT
#define BTN_B 37   
//#define BTN_C 36

static void keypad_read(lv_indev_t * indev, lv_indev_data_t * data) {
   static uint8_t btn_a_last = HIGH;
    static uint8_t btn_b_last = HIGH;

    uint8_t a = digitalRead(BTN_A);
    uint8_t b = digitalRead(BTN_B);

    data->state = LV_INDEV_STATE_REL; // default released
    data->key = 0;

    // Modifier logic
    // if (a == LOW ) { // A is pressed
    //     if (b == LOW && btn_a_last == HIGH) {
    //         // A pressed while B held
	// 		outputString("A pressed while B held");
    //         data->key = LV_KEY_LEFT;
    //         data->state = LV_INDEV_STATE_PR;
    //     } else if (btn_a_last == HIGH) {
    //         // A pressed alone (just pressed)
	// 		outputString("A pressed alone");
    //         data->key = LV_KEY_NEXT;
    //         data->state = LV_INDEV_STATE_PR;
    //     }
    // } else if (b == LOW) { // B pressed
    //     if (a == LOW &&   btn_b_last == HIGH) {
    //         // B pressed while A held
	// 		outputString("B pressed while A held");
    //         data->key = LV_KEY_RIGHT;
    //         data->state = LV_INDEV_STATE_PR;
    //     } else if (btn_b_last == HIGH) {
    //         // B pressed alone (just pressed)
	// 		outputString("B pressed alone");
    //         data->key = LV_KEY_ENTER;
    //         data->state = LV_INDEV_STATE_PR;
    //     }
    // }

// --- Check A press ---
    if (a == LOW && btn_a_last == HIGH) {
        if (b == LOW) {
            // A pressed while B held
            outputString("A pressed while B held");
            data->key = LV_KEY_LEFT;
        } else {
            // A pressed alone
            outputString("A pressed alone");
            data->key = LV_KEY_NEXT;
        }
        data->state = LV_INDEV_STATE_PR;
    }

    // --- Check B press ---
    if (b == LOW && btn_b_last == HIGH) {
        if (a == LOW) {
            // B pressed while A held
            outputString("B pressed while A held");
            data->key = LV_KEY_RIGHT;
        } else {
            // B pressed alone
            outputString("B pressed alone");
            data->key = LV_KEY_ENTER;
        }
        data->state = LV_INDEV_STATE_PR;
    }


  	if (a == HIGH && b == HIGH) {
        data->state = LV_INDEV_STATE_REL;
    }

    btn_a_last = a;
    btn_b_last = b;
	// btn_c_last = c;
}



#endif


#if defined(HAS_TOUCH_SCREEN)
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {
    /*
    uint16_t x_raw,y_raw;
     if (ts.touched()) {
    TS_Point p = ts.getPoint();
    // Map raw touch to screen coordinates
    data->state = LV_INDEV_STATE_PR;
    data->point.x = map(p.x, 200, 3800, 0, TFT_WIDTH);
    data->point.y = map(p.y, 250, 3900,  TFT_HEIGHT,0);
	*/
	// use global functions
	if (screenTouched()) {
		data->state = LV_INDEV_STATE_PR;
    	data->point.x = screenTouchX();
    	data->point.y = screenTouchY();
	
    } else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif
#endif

#if defined(LVGL)




#define TFT_BUFFER_LINES 40
static lv_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;
//static lv_color_t buf[TFT_WIDTH * TFT_BUFFER_LINES];

#if defined(COCUBE)
lv_group_t * group;
#endif


bool event_seen = false;

static lv_display_t * disp;


// some includes for c++ maps etc.
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>


template<typename T>
class ObjectRegistry {
public:
    void add(const std::string& name, T* obj) {
        registry[name] = obj;
    }

	

    T* get(const std::string& name) const {
		auto it = registry.find(name);
        return it != registry.end() ? it->second : nullptr;
	}

    bool remove(const std::string& name) {
        return registry.erase(name) > 0;
    }

    void printall() {
         for (const auto& pair : registry) {
             char s[100];
			sprintf(s,"name %s ",pair.first.c_str());
			outputString(s);
    	}
	}

    size_t size() const {
        return registry.size();
    }

	std::vector<std::string> getAllNames() const {
        std::vector<std::string> names;
        for (const auto& entry : registry) {
            names.push_back(entry.first);
        }
        return names;
    }

	std::string findNameFor(T* obj) const {
        for (const auto& pair : registry) {
            if (pair.second == obj) {
                return pair.first;
				//  char s[100];
				// sprintf(s,"find name %s ",pair.first.c_str());
				// outputString(s);
            }
        }
        return "";
    }

private:
    std::unordered_map<std::string, T*> registry;
};

template <>
lv_obj_t* ObjectRegistry<lv_obj_t>::get(const std::string& name) const {
    if (name == "lv_scr_act") {
        return lv_scr_act();
    } else {
        auto it = registry.find(name);
        return it != registry.end() ? it->second : nullptr;
    }
}


ObjectRegistry<lv_obj_t>  registry;

ObjectRegistry<lv_font_t> font_buffer;

ObjectRegistry<lv_style_t> style_registry;

ObjectRegistry<lv_chart_series_t> series_registry;

ObjectRegistry<char*> btnmap_registry;

 
void fs_init() {

    if (!LittleFS.begin()) {
        outputString("⚠️ LittleFS mount failed, formatting...");
        if (!LittleFS.format()) {
            outputString("❌ LittleFS format failed!");
        }
        if (!LittleFS.begin()) {
            outputString("❌ LittleFS mount failed again after format!");
        }
    }
    outputString("✅ LittleFS mounted successfully.");

}

/*
bool my_ready_cb(lv_fs_drv_t *) {
  return true;
}

void *my_open_cb(lv_fs_drv_t *, const char *path, lv_fs_mode_t mode) {
  char full_path[64];
  snprintf(full_path, sizeof(full_path), "/%s", path);
  const char *fmode = (mode == LV_FS_MODE_WR) ? "w" : "r";
  File *f = new File(LittleFS.open(full_path, fmode));
  if (!f || !*f) {
    delete f;
    return nullptr;
  }
  outputString("file oped");
  outputString(full_path);
  return f;
}

lv_fs_res_t my_close_cb(lv_fs_drv_t *, void *file_p) {
  File *f = static_cast<File *>(file_p);
  f->close();
  delete f;
  return LV_FS_RES_OK;
}

lv_fs_res_t my_read_cb(lv_fs_drv_t *, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
  File *f = static_cast<File *>(file_p);
  *br = f->read((uint8_t *)buf, btr);
  uint8_t *cp = (uint8_t*)br;
  return LV_FS_RES_OK;
}

lv_fs_res_t my_seek_cb(lv_fs_drv_t *, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
  File *f = static_cast<File *>(file_p);
  if (whence == LV_FS_SEEK_CUR) f->seek(pos + f->position());
  else if (whence == LV_FS_SEEK_END) f->seek(f->size() - pos);
  else f->seek(pos);
  return LV_FS_RES_OK;
}

lv_fs_res_t my_tell_cb(lv_fs_drv_t *, void *file_p, uint32_t *pos) {
  File *f = static_cast<File *>(file_p);
  *pos = f->position();
  return LV_FS_RES_OK;
}

// Register LittleFS with LVGL
void lv_fs_littlefs_init() {
  static lv_fs_drv_t drv;
  lv_fs_drv_init(&drv);
  drv.letter = 'L';
  drv.ready_cb = my_ready_cb;
  drv.open_cb = my_open_cb;
  drv.close_cb = my_close_cb;
  drv.read_cb = my_read_cb;
  drv.seek_cb = my_seek_cb;
  drv.tell_cb = my_tell_cb;
  lv_fs_drv_register(&drv);
}


uint8_t* load_file_to_psram(const char *path, size_t *out_size) {
    // Open the file
    fs::File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) {
        outputString("Failed to open file for reading");
        return nullptr;
    }

    size_t size = f.size();  // Get the file size
    if (out_size) *out_size = size;

    // Allocate buffer in PSRAM
    //uint8_t *buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	uint8_t *buffer = (uint8_t *)malloc(size);
	
    if (!buffer) {
        outputString("Failed to allocate PSRAM buffer");
        f.close();
        return nullptr;
    }

    // Read file into buffer
    size_t bytes_read = f.read(buffer, size);
	char s[100];
    sprintf(s,"Read %u bytes\n", bytes_read);
	outputString(s);
    f.close();

    if (bytes_read != size) {
		//char s[100];
        sprintf(s,"Read %u/%u bytes\n", bytes_read, size);
		outputString(s);
        heap_caps_free(buffer);
        return nullptr;
    }

    return buffer;
}
*/
/* //sodb task to run lvgl on core 1; does not work
void lvglTask(void *pvParameter) {
    while (true) {
        if (LVGL_initialized && useLVGL) {
            lv_tick_inc(5);        // advance LVGL tick
            lv_timer_handler();    // process LVGL tasks
        }
        //vTaskDelay(5 / portTICK_PERIOD_MS);  // let other tasks run
		vTaskDelay(5);  // let other tasks run
    }
}
*/

void setup_lvgl() {
	/*
	#include "esp_heap_caps.h"

 	size_t buf_size = TFT_WIDTH * TFT_BUFFER_LINES * sizeof(lv_color_t);
 	char s[100];
	 sprintf(s,"free heap before: %d psram: %d ",  ESP.getFreeHeap(),ESP.getFreePsram());
	 outputString(s);
    

	 buf = (lv_color_t *)malloc(TFT_WIDTH * TFT_BUFFER_LINES * sizeof(lv_color_t));
	
	 //buf = (lv_color_t *)heap_caps_malloc(TFT_WIDTH * 10 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	 if (buf)  outputString("malloc succesfull");
	 else outputString("cannot mallocsuccesfull");
	
	  sprintf(s,"free heap after: %d psram: %d ",  ESP.getFreeHeap(),ESP.getFreePsram());
	  outputString(s);
	  */
  	lv_init();
	// double buffer

/* // sodb try to give Wifi more chance 
xTaskCreatePinnedToCore(
        lvglTask,       // Task function
        "LVGL Task",    // Name
        64000,           // Stack size (increase if widgets crash)
        NULL,           // Parameters
        1,              // Priority
        NULL,           // Handle
        1               // Core 1
    );

*/
#include "esp_heap_caps.h"

 	size_t buf_size = TFT_WIDTH * TFT_BUFFER_LINES * sizeof(lv_color_t);
 	char s[100];
	 sprintf(s,"free heap before: %d psram: %d ",  ESP.getFreeHeap(),ESP.getFreePsram());
	 outputString(s);
    
	  
	#if defined(COCUBE)
	  buf1 = (lv_color_t *)heap_caps_malloc(TFT_WIDTH * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
 	  buf2 = (lv_color_t *)heap_caps_malloc(TFT_WIDTH * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
	#else
	  buf1 = (lv_color_t *)heap_caps_malloc(TFT_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
 	  buf2 = (lv_color_t *)heap_caps_malloc(TFT_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
	#endif
 	if (buf1)  outputString("malloc succesfull");
	 else outputString("cannot mallocsuccesfull");
	
	  sprintf(s,"free heap after: %d psram: %d ",  ESP.getFreeHeap(),ESP.getFreePsram());
	  outputString(s);

	disp = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_buffers(disp, buf1, buf2, TFT_WIDTH * 40, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);
	#if defined(TFT_ESPI) 
		#if defined(CYDROT)
			lv_display_set_resolution(disp, TFT_WIDTH, TFT_HEIGHT);
		#else
			lv_display_set_resolution(disp, TFT_HEIGHT, TFT_WIDTH);
		#endif
	#else
    	lv_display_set_resolution(disp, TFT_WIDTH, TFT_HEIGHT);
	#endif
   #if defined(HAS_TOUCH_SCREEN)
	/*Initialize the (dummy) input device driver*/
		lv_indev_t * indev = lv_indev_create();
		lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
		lv_indev_set_read_cb(indev, my_touchpad_read);
		if (!touchEnabled) touchInit();
	#endif
   #if defined(COCUBE)
    lv_indev_t * indev = lv_indev_create();
	lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD); /*Touchpad should have POINTER type*/
	lv_indev_set_read_cb(indev,keypad_read);
	
	lv_indev_set_long_press_time(indev, 400);        // ms until LV_EVENT_LONG_PRESSED
	lv_indev_set_long_press_repeat_time(indev, 100); // repeat interval in ms

    // Optional: create a group so widgets can get focus
	group = lv_group_create();
	// Attach the keypad input device to the group
	lv_indev_set_group(indev, group);
   #endif
 fs_init() ;
//lv_fs_littlefs_init();
	LVGL_initialized = true;
	// store main screen object in object with name '!main_screen_default'
	// hide this object from user in get_all_objects
	registry.add("!main_screen_default",lv_scr_act() );
}


void set_lvgl(bool use_lvgl) {
	if (use_lvgl) {
		useLVGL=true;
		// refresh all objects

		lv_obj_invalidate(lv_scr_act());

		// char s[100];
		// sprintf(s,"set lvgl on %d\n",use_lvgl);
		// outputString(s);

	} else {
		useLVGL=false;
		tftClear();
		// char s[100];
		// sprintf(s,"set tft on \n");
		// outputString(s);
	}
}


// dummy test for generating ticks
void lvgl_tick() {
 	lv_tick_inc(1);
     lv_timer_handler();
}



// in lvgl 9 there is no LV_EVENT_NONE defined, so define it ourselves
#define LV_EVENT_NONE_CUSTOM (lv_event_code_t)(-1)
struct LastEventInfo {
    lv_event_t *event;
    lv_event_code_t code;
    lv_obj_t *target;
	std::string  name;
	uint32_t id;
};


static LastEventInfo last_event = {
    nullptr,
    LV_EVENT_NONE_CUSTOM,
    nullptr,
    std::string(),   // default empty string
    0                // id
};

// generic call back function for all events
void ui_log_event_cb(lv_event_t *e) {
    last_event.event = e;
    last_event.code = lv_event_get_code(e);
    last_event.target = (lv_obj_t *) lv_event_get_target(e);
	if (lv_obj_get_class(last_event.target) == &lv_buttonmatrix_class) {
		last_event.id = lv_buttonmatrix_get_selected_button(last_event.target);
	}
	last_event.name = registry.findNameFor( last_event.target);
		char s[100];
		sprintf(s,"Event %d on obj name %s id %d", last_event.code, last_event.name.c_str(),last_event.id);
		outputString(s);
	// // send broadcast
	event_seen = true; // set to false in getevent
	
	char eventmessage[] = "LVGLevent";
	// send a broadcast with text: LVGLevent
	startReceiversOfBroadcast(eventmessage, 9);
	sendBroadcastToIDE(eventmessage, 9);
}

int ui_get_last_event(std::string& name_out) {
    name_out = last_event.name;
    return static_cast<int>(last_event.code);
}


const lv_font_t* get_font_from_scale(int scale_x) {
    switch(scale_x) {
        case 1: return &lv_font_montserrat_14;
        case 2: return &lv_font_montserrat_24;
        case 3: return &lv_font_montserrat_40;
		case 4: return &lv_font_montserrat_48;
        default: return &lv_font_montserrat_14; // default fallback
    }
}




void ui_add_image(char * obj_name, const char *path, const char * parent_name) {
	lv_obj_t* parent = registry.get(parent_name);
	lv_obj_t* obj;
    // Create an img object
    
	if (!registry.get(obj_name) && parent) {
		lv_obj_t *obj = lv_img_create(lv_screen_active());
		// Set image source from file
		lv_img_set_src(obj, path);
		outputString("ui_add_image");
		outputString(path);
		// Optional: align or move the image
		//lv_obj_center(img);
		registry.add(obj_name, obj);
	}
}


void ui_add_font(char * obj_name, const char *path) {
	lv_obj_t* obj;
    // Create an img object
    
	if (!font_buffer.get(obj_name) ) {
		lv_font_t *obj = lv_binfont_create(path);
		// Set image source from file
		if (obj) {
			font_buffer.add(obj_name, obj);
		}
	}
}

/*
void ui_add_image(char * obj_name, const char *path, const char * parent_name) {
	lv_obj_t* parent = registry.get(parent_name);
	lv_obj_t* obj;
    // Create an img object
    
	if (!registry.get(obj_name) && parent) {
		size_t size;
		uint8_t* buffer = load_file_to_psram(path,&size);
		lv_image_dsc_t *img_dsc = (lv_image_dsc_t *)buffer;
		lv_obj_t *obj = lv_img_create(lv_screen_active());
		// Set image source from file
		lv_img_set_src(obj, img_dsc);
		outputString("ui_add_image");
		outputString(path);
		// Optional: align or move the image
		//lv_obj_center(img);
		registry.add(obj_name, obj);
		img_buffer.add(obj_name, buffer);

	}
}

*/


void ui_create_button_label(char * obj_name, int scale, const char * label_text, const char * parent_name) {
	lv_obj_t* parent = registry.get(parent_name);
	lv_obj_t* obj;
	if (!registry.get(obj_name) && parent) {
		if (lv_obj_get_class(parent) == &lv_list_class) {
			obj = lv_list_add_button(parent, NULL, label_text);
		} else {
			obj = lv_btn_create(parent);
			// sodb: check whether label is correctly removed when parent btn object is deleted
			lv_obj_t * label = lv_label_create(obj);
			lv_label_set_text(label, label_text);
			lv_obj_set_style_text_font(label, get_font_from_scale(scale), LV_PART_MAIN);
			lv_obj_center(label);
		}
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_CLICKED, NULL);
		registry.add(obj_name, obj);
	}

}


void ui_create_button(char * obj_name, const char * parent) {
	if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_btn_create(registry.get(parent));
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_CLICKED, NULL);
		// sodb: check whether label is correctly removes when partent btn object is deleted
		registry.add(obj_name, obj);
	}

}

void ui_create_label(char * obj_name, int scale, const char * label_text, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* label = lv_label_create(registry.get(parent));
		lv_label_set_text(label, label_text);
		lv_obj_set_style_text_font(label, get_font_from_scale(scale), LV_PART_MAIN);
		registry.add(obj_name, label);
	}
}


void ui_create_slider(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_slider_create(registry.get(parent));
		lv_obj_add_event_cb(obj, ui_log_event_cb,LV_EVENT_VALUE_CHANGED, NULL);
		lv_obj_add_event_cb(obj, ui_log_event_cb,LV_EVENT_LONG_PRESSED, NULL);
		lv_slider_set_range(obj, 0, 100);
		registry.add(obj_name, obj);
	}
}

void ui_create_arc(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_arc_create(registry.get(parent));
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		lv_obj_add_event_cb(obj, ui_log_event_cb,LV_EVENT_LONG_PRESSED, NULL);
		// sodb solve unmovable arc on capacitive touch displays.
		lv_obj_add_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE ));
		registry.add(obj_name, obj);
	}
}


void ui_create_switch(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_switch_create(registry.get(parent));
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}


void ui_create_led(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_led_create(registry.get(parent));
		// lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}

void ui_create_bar(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_bar_create(registry.get(parent));
		// lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}

void ui_create_tabview(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_tabview_create(registry.get(parent));
		// lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}

void ui_create_tileview(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_tileview_create(registry.get(parent));
		// lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}

void ui_create_screen(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		#if defined(LMSDISPLAY)
			#define _TFT_WIDTH TFT_HEIGHT
			#define _TFT_HEIGHT TFT_WIDTH
		#else
			#define _TFT_WIDTH TFT_WIDTH
			#define _TFT_HEIGHT TFT_HEIGHT
		#endif
		lv_obj_t* obj = lv_obj_create(0); // crete empty screen
		lv_obj_set_pos(obj, 0, 0);
		lv_obj_set_size(obj, _TFT_WIDTH, _TFT_HEIGHT);
		registry.add(obj_name, obj);
	}
}


void ui_create_roller(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_roller_create(registry.get(parent));
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		lv_obj_add_event_cb(obj, ui_log_event_cb,LV_EVENT_LONG_PRESSED, NULL);
		registry.add(obj_name, obj);
	}
}

void ui_create_spinbox(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_spinbox_create(registry.get(parent));
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}

void ui_create_spinner(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_spinner_create(registry.get(parent));
		registry.add(obj_name, obj);
	}
}

void ui_create_scale(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_scale_create(registry.get(parent));
		registry.add(obj_name, obj);
	}
}

void ui_create_keyboard(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_keyboard_create(registry.get(parent));
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_READY, NULL);
		registry.add(obj_name, obj);
	}
}
void ui_create_textarea(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_textarea_create(registry.get(parent));
		registry.add(obj_name, obj);
	}
}

void ui_add_tab(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_tabview_add_tab(registry.get(parent),obj_name);
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}

void ui_add_series(char * series, const char * chart, int color) {
    if (registry.get(chart) && !series_registry.get(series)) {
		lv_chart_series_t* obj = lv_chart_add_series(registry.get(chart), lv_color_hex(color),  LV_CHART_AXIS_PRIMARY_Y);
		//lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		outputString("ui_add_series");
		series_registry.add(series, obj);
	}
}

void ui_set_next_value(char * series, char * chart, int val) {
    if (registry.get(chart) && series_registry.get(series)) {
		lv_chart_set_next_value(registry.get(chart), series_registry.get(series), val);
		char s[100];
		sprintf(s,"ui_set_next_value series=%s chart=%s val = %d", series,chart,val);
		outputString(s);

	}
}

void ui_set_next_value2(char * series, char * chart, int val, int val2) {
    if (registry.get(chart) && series_registry.get(series)) {
		lv_chart_set_next_value2(registry.get(chart), series_registry.get(series), val, val2);
		outputString("lv_chart_set_next_value2");
	}
}


void ui_add_chart(char * obj_name, const char * parent, char * chart_type, char * chart_update_mode) {
	if (!registry.get(obj_name))  {
		lv_chart_type_t chart_type_id = LV_CHART_TYPE_LINE;
		lv_chart_update_mode_t chart_update_mode_id = LV_CHART_UPDATE_MODE_SHIFT;
		if (strcmp(chart_type,"bar")==0) chart_type_id = LV_CHART_TYPE_BAR;
		else if (strcmp(chart_type,"scatter")==0) chart_type_id = LV_CHART_TYPE_SCATTER;

		if (strcmp(chart_update_mode,"circular")==0) chart_update_mode_id = LV_CHART_UPDATE_MODE_CIRCULAR;
		lv_obj_t* obj = lv_chart_create(registry.get(parent));
		lv_chart_set_type(obj, chart_type_id);
		lv_chart_set_update_mode(obj, chart_update_mode_id);

		registry.add(obj_name, obj);
	}
}

void ui_add_tile(char * obj_name, const char * parent, int col_id, int row_id, lv_dir_t dir ) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_tileview_add_tile(registry.get(parent), col_id, row_id, dir);
		lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
}


void ui_create_list(char * obj_name, const char * parent) {
    if (!registry.get(obj_name) && registry.get(parent)) {
		lv_obj_t* obj = lv_list_create(registry.get(parent));
		// lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		registry.add(obj_name, obj);
	}
	if (strcmp(parent,"lv_scr_act") !=0) {
		
	}
}

void ui_create_style(char * obj_name, const char * parent) {
    if (!style_registry.get(obj_name)) { 
		lv_style_t* obj = new lv_style_t;
		lv_style_init(obj);
		// lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		style_registry.add(obj_name, obj);
	}
}

void ui_set_parent(char * obj_name, const char * parent, int states, int parts){
	lv_obj_t* obj =  registry.get(obj_name);
	lv_obj_t* obj_parent =  registry.get(parent);
	char s[100];
	sprintf(s,"set parent: %s parent %s",obj_name,parent);
	outputString(s);

	if (obj && obj_parent) {
		if ( (lv_obj_get_class(obj) == &lv_keyboard_class) &&
			 (lv_obj_get_class(obj_parent) == &lv_textarea_class) ) {
				outputString("attaching keyboard to textarea");
			 	lv_keyboard_set_textarea(obj, obj_parent);
			} else
				lv_obj_set_parent(registry.get(obj_name), obj_parent);
	} else
	if (style_registry.get(obj_name) && obj_parent) {
		lv_style_t* style =  style_registry.get(obj_name);
		lv_obj_add_style(obj_parent, style, states + parts);
		outputString("style added");
		
	}
}


void free_btnmap(char **btnmap) {
    if (!btnmap) return;
    for (size_t i = 0; btnmap[i] != NULL; i++) {
        free(btnmap[i]);
    }
    free(btnmap);
}

void ui_delete_obj(char * obj_name) {
    lv_obj_t* obj = registry.get(obj_name);
	lv_font_t* font = font_buffer.get(obj_name);
	lv_style_t* style = style_registry.get(obj_name);
	lv_chart_series_t* chart_series = series_registry.get(obj_name);
	char** btnmap = btnmap_registry.get(obj_name);
	// lv_chart_series_t* series = series_registry.get(obj_name); 
	// not needed because lv_obj_del of chart already deletes all the series attached to the chart
    if (obj) {
		lv_obj_del(obj);
        registry.remove(obj_name);
    } 
	if (font){
			outputString("deleting font");
			lv_binfont_destroy(font);
			font_buffer.remove(obj_name);
	} 
	if (style) { 
		delete style;
		style_registry.remove(obj_name);
	} 
	if (chart_series) {
		// lv_obj_del(obj); // slready deleted iwith parent chart
        series_registry.remove(obj_name);
	}
	if (btnmap) { 
		outputString("deleting btnmap");
		free_btnmap(btnmap); // free structure of char** for btnmap
		btnmap_registry.remove(obj_name); // remove entry in btnmap_registry
	}
}

void ui_set_size(char * obj_name,  lv_coord_t w, lv_coord_t h ) {
	lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		// char s[100];
		// sprintf(s,"set_size %s, x=%d, y=%d",obj_name,w,h);
		// outputString(s);
		lv_obj_set_size(obj,w,h);
	}
}

void ui_set_pos(char * obj_name,  uint16_t pos_x, uint16_t pos_y) {
	lv_obj_t* obj = registry.get(obj_name);
	if (obj!=nullptr) {
		lv_obj_set_pos(obj,pos_x,pos_y);
	}
}

void ui_set_scroll(char * obj_name,  lv_dir_t scroll_dir) {
	lv_obj_t* obj = registry.get(obj_name);
	if (obj!=nullptr) {
		if (lv_obj_get_class(obj) == &lv_tabview_class) {
			// when tabview, take container of tabs to controll scroll direction
			lv_obj_t *content = lv_tabview_get_content(obj);
			lv_obj_set_scroll_dir(content, scroll_dir);
		} else
			  lv_obj_set_scroll_dir(obj, scroll_dir);
		}
}


void ui_set_value(char * obj_name, int value) {
	lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		if (lv_obj_get_class(obj) == &lv_arc_class) {
			lv_arc_set_value(obj, value);
		} else 
		if (lv_obj_get_class(obj) == &lv_slider_class) {
			lv_slider_set_value(obj, value, LV_ANIM_OFF);
		} else
		if (lv_obj_get_class(obj) == &lv_bar_class) {
			lv_bar_set_value(obj, value, LV_ANIM_OFF);
		} else
		if (lv_obj_get_class(obj) == &lv_spinbox_class) {
		char s[100];
		sprintf(s,"set_value %s: %d",obj_name,value);
		outputString(s);
			lv_spinbox_set_value(obj, value);
		} else
		if (lv_obj_get_class(obj) == &lv_switch_class) {
			if (value==0) lv_obj_remove_state(obj, LV_STATE_CHECKED);
			else if (value&1) lv_obj_add_state(obj, LV_STATE_CHECKED);
			else if (value>1) lv_obj_add_state(obj, (lv_state_t)value);
			else if (value<0) lv_obj_remove_state(obj,(lv_state_t) -value);
			
		} else
		if (lv_obj_get_class(obj) == &lv_led_class) {
			if (value==0) lv_led_off(obj);
			else if (value&1) lv_led_on(obj);
			
			
		} else
		if (lv_obj_get_class(obj) == &lv_roller_class) {
			lv_roller_set_visible_row_count(obj,value);
		}
	}
}

void ui_set_text(char * obj_name, char * text, int scale) {
    lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		if (lv_obj_get_class(obj) == &lv_label_class) {
			lv_label_set_text(obj, text);
			lv_obj_set_style_text_font(obj, get_font_from_scale(scale), LV_PART_MAIN);
		} else 
		if (lv_obj_get_class(obj) == &lv_button_class) {
			lv_obj_t *label = lv_obj_get_child(obj, 0);
			if (label) {
				lv_label_set_text(label, text);
				lv_obj_set_style_text_font(label, get_font_from_scale(scale), LV_PART_MAIN);
			}
		} else 
		if (lv_obj_get_class(obj) == &lv_roller_class) {
			lv_roller_set_options(obj, text, LV_ROLLER_MODE_INFINITE);
			lv_obj_set_style_text_font(obj, get_font_from_scale(scale), LV_PART_MAIN | LV_STATE_DEFAULT | LV_STYLE_PROP_FLAG_INHERITABLE);
			lv_obj_set_style_text_font(obj, get_font_from_scale(scale), LV_PART_SELECTED|  LV_STATE_DEFAULT);
			
		}else 
		if (lv_obj_get_class(obj) == &lv_textarea_class) {
			lv_textarea_set_text(obj, text);
			//lv_obj_set_style_text_font(obj, get_font_from_scale(scale), LV_PART_MAIN); // does not seem to work
		}
	}
}

void ui_set_text_font(char * obj_name, char * text, char * font_name) {
    lv_obj_t* obj = registry.get(obj_name);

				
	if (obj) {
		lv_font_t * font = font_buffer.get(font_name);
		if (lv_obj_get_class(obj) == &lv_label_class) {
			lv_label_set_text(obj, text);
			if (font) {
				lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
			}
		} else 
		if (lv_obj_get_class(obj) == &lv_button_class) {
			lv_obj_t *label = lv_obj_get_child(obj, 0);
			if (label) {
				lv_label_set_text(label, text);
				if (font)
					lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
			}
		} else 
		if (lv_obj_get_class(obj) == &lv_roller_class) {
			lv_roller_set_options(obj, text, LV_ROLLER_MODE_INFINITE);
		}
	}
}


void ui_set_attribute(char * obj_name, char * attribute_name, int to_val, int until_val){
	lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		if (strcmp(attribute_name,"flags")==0) lv_obj_add_flag(obj, (lv_obj_flag_t)to_val); else
		if (lv_obj_get_class(obj) == &lv_arc_class) {
			if (strcmp(attribute_name,"range")==0) lv_arc_set_range(obj, to_val, until_val);
			else if (strstr(attribute_name,"angles")) lv_arc_set_bg_angles(obj, to_val, until_val);
			else if (strstr(attribute_name,"rotation")) lv_arc_set_rotation(obj, to_val);
			else if (strstr(attribute_name,"line width")) {
				outputString("line width");
				lv_obj_set_style_arc_width(obj,to_val,LV_PART_MAIN);
				lv_obj_set_style_arc_width(obj,to_val,LV_PART_INDICATOR);
			}
		} else 
		if (lv_obj_get_class(obj) == &lv_slider_class) {
			if (strcmp(attribute_name,"range")==0) lv_slider_set_range(obj, to_val, until_val);
		} else
		if (lv_obj_get_class(obj) == &lv_bar_class) {
			if (strcmp(attribute_name,"range")==0) lv_bar_set_range(obj, to_val, until_val);
		} else
		if (lv_obj_get_class(obj) == &lv_spinner_class) {
			if (strcmp(attribute_name,"animation")==0) lv_spinner_set_anim_params(obj, to_val, until_val);
			else if (strstr(attribute_name,"line width")) {
				lv_obj_set_style_arc_width(obj,to_val,LV_PART_MAIN);
				lv_obj_set_style_arc_width(obj,to_val,LV_PART_INDICATOR);
			}
		} else
		if (lv_obj_get_class(obj) == &lv_spinbox_class) {
			if (strcmp(attribute_name,"range")==0) lv_spinbox_set_range(obj, to_val, until_val);
			if (strcmp(attribute_name,"digits")==0) lv_spinbox_set_digit_format(obj, to_val, until_val);
			if (strcmp(attribute_name,"increment")==0) lv_spinbox_increment(obj);
			if (strcmp(attribute_name,"decrement")==0) lv_spinbox_decrement(obj);
		} else
		if (lv_obj_get_class(obj) == &lv_led_class) {
			if (strcmp(attribute_name,"brightness")==0) {
				lv_led_set_brightness(obj,to_val );
			}
		} else 
		if (lv_obj_get_class(obj) == &lv_chart_class) {
			if (strcmp(attribute_name,"points")==0) {
				lv_chart_set_point_count(obj,to_val );
				outputString("lv_chart_set_point_count");
			}
			if (strcmp(attribute_name,"range")==0) {
				lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, to_val, until_val);
			}
		} else
		if (lv_obj_get_class(obj) == &lv_scale_class) {
			if (strcmp(attribute_name,"tick count")==0) lv_scale_set_total_tick_count(obj,to_val );
			if (strcmp(attribute_name,"major tick every")==0) lv_scale_set_major_tick_every(obj,to_val );
			if (strcmp(attribute_name,"length")==0) lv_obj_set_style_length(obj,to_val, until_val );
			if (strcmp(attribute_name,"range")==0) lv_scale_set_range(obj, to_val, until_val);
			if (strstr(attribute_name,"scale mode")) lv_scale_set_mode(obj, (lv_scale_mode_t) to_val);
			if (strstr(attribute_name,"angles")) lv_scale_set_angle_range(obj, to_val);
			if (strstr(attribute_name,"rotation")) lv_scale_set_rotation(obj, to_val);
			if (strcmp(attribute_name,"show labels")==0) {
				bool show_labels = (to_val==1);
				lv_scale_set_label_show(obj,show_labels);
			}
		} else
		if (lv_obj_get_class(obj) == &lv_buttonmatrix_class) {
			if (strcmp(attribute_name,"button ctrl")==0) {
				// first clear all states
				lv_btnmatrix_clear_btn_ctrl(obj, to_val, LV_BTNMATRIX_CTRL_HIDDEN);
				lv_btnmatrix_clear_btn_ctrl(obj, to_val, LV_BTNMATRIX_CTRL_DISABLED);
				lv_btnmatrix_clear_btn_ctrl(obj, to_val, LV_BTNMATRIX_CTRL_CHECKED);
				lv_btnmatrix_clear_btn_ctrl(obj, to_val, LV_BTNMATRIX_CTRL_CHECKABLE);
				lv_buttonmatrix_set_button_ctrl(obj,to_val, (lv_buttonmatrix_ctrl_t)until_val); // id, button_ctrl
			}
			if (strcmp(attribute_name,"width")==0) lv_buttonmatrix_set_button_width(obj,to_val, until_val); // id, width
		} else 
		if (lv_obj_get_class(obj) == &lv_textarea_class) {
			if (strcmp(attribute_name,"focused")==0) lv_obj_add_state(obj, LV_STATE_FOCUSED);
		} else
		if ((lv_obj_get_class(obj) == &lv_button_class)  || (lv_obj_get_class(obj) == &lv_label_class)) {
		   if (strstr(attribute_name,"rotation")) lv_obj_set_style_transform_angle(obj, to_val, 0);
		}
	}
}


void ui_set_style(char * obj_name, char * style_name, int to_val){
	lv_style_t* obj = style_registry.get(obj_name);
	if (obj) {
		if (strcmp(style_name,"text font")==0) lv_style_set_text_font(obj,  get_font_from_scale(to_val));
			else if (strstr(style_name,"bg color")) lv_style_set_bg_color(obj, lv_color_hex(to_val));
			else if (strstr(style_name,"bg opa")) lv_style_set_bg_opa(obj, to_val);
			else if (strstr(style_name,"border width")) lv_style_set_border_width(obj, to_val);
    		else if (strstr(style_name,"border color")) lv_style_set_border_color(obj, lv_color_hex(to_val));
			else if (strstr(style_name,"radius")) lv_style_set_radius(obj, to_val);
			else if (strstr(style_name,"shadow width")) lv_style_set_shadow_width(obj, to_val);
			else if (strstr(style_name,"shadow offset x")) lv_style_set_shadow_offset_x(obj, to_val);
			else if (strstr(style_name,"shadow offset y")) lv_style_set_shadow_offset_y(obj, to_val);
			else if (strstr(style_name,"shadow opa")) lv_style_set_shadow_opa(obj, to_val);
			else if (strstr(style_name,"width")) lv_style_set_width(obj, to_val);
			else if (strstr(style_name,"line width")) lv_style_set_line_width(obj, to_val);
			else if (strstr(style_name,"line color")) lv_style_set_line_color(obj, lv_color_hex(to_val));
			else if (strstr(style_name,"text color")) lv_style_set_text_color(obj, lv_color_hex(to_val));


			

	}
}


struct ClassNameMap {
    const lv_obj_class_t *cls;
    const char *name;
} class_map[] = {
    { &lv_buttonmatrix_class, "buttonmatrix" },
    { &lv_label_class,       "label" },
    { &lv_button_class,         "button" },
    { &lv_obj_class,         "generic_obj" }, // base class
    { nullptr,               nullptr }
};

// Get a readable class name
const char *get_class_name(const lv_obj_class_t *cls) {
    for (int i = 0; class_map[i].cls; i++) {
        if (class_map[i].cls == cls) return class_map[i].name;
    }
    return "(unknown)";
}

// Print class hierarchy
void print_class_hierarchy(const lv_obj_t *obj) {
    if (!obj) return;

    const lv_obj_class_t *cls = lv_obj_get_class(obj);

    char s[100];
			sprintf(s,"class name - %s (%p)", get_class_name(cls), cls);
			outputString(s);
}

void ui_set_color(char * obj_name, int color) {
	lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		if (lv_obj_get_class(obj) == &lv_label_class) {
			lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN); 
		} else 
		if  (lv_obj_get_class(obj) == &lv_led_class) {
			lv_led_set_color(obj,lv_color_hex(color));
		} else
		if  (lv_obj_get_class(obj) == &lv_switch_class) {
			lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN  | LV_STATE_DEFAULT);
			lv_obj_set_style_bg_opa(obj, LV_OPA_COVER,LV_PART_MAIN |LV_STATE_DEFAULT);
	    } else
		if  ((lv_obj_get_class(obj) == &lv_arc_class) || (lv_obj_get_class(obj) == &lv_spinner_class)) {
			outputString("change color arc or spinner");
			lv_obj_set_style_arc_color(obj, lv_color_hex(color), LV_PART_MAIN);
 		} else
			lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
	
	}
}

void ui_set_color_2nd(char * obj_name, int color) {
	lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		if (lv_obj_get_class(obj) == &lv_button_class) {
			lv_obj_t *label = lv_obj_get_child(obj, 0);
			lv_obj_set_style_text_color(label, lv_color_hex(color), 0); 
		} else if ((lv_obj_get_class(obj) == &lv_arc_class)  || (lv_obj_get_class(obj) == &lv_spinner_class)){
			lv_obj_set_style_arc_color(obj, lv_color_hex(color), LV_PART_INDICATOR);
		} else if (lv_obj_get_class(obj) == &lv_switch_class) {
			lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_INDICATOR|LV_STATE_CHECKED);
		    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR|LV_STATE_CHECKED);
		}
 		else 
			lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_INDICATOR);
		
	}
}

void ui_set_color_3rd(char * obj_name, int color) {
	lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_KNOB);
		if (lv_obj_get_class(obj) == &lv_switch_class) lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_KNOB);
	}
}

void printall() {
	registry.printall();

}


// helper code for object selection

typedef enum {
    CMD_UNKNOWN = -1,
    CMD_BUTTON,
	CMD_LABEL,
    CMD_ARC,
    CMD_SLIDER,
    CMD_LED,
    CMD_SWITCH,
	CMD_BAR,
	CMD_TABVIEW,
	CMD_TILEVIEW,
	CMD_LIST,
	CMD_ROLLER,
	CMD_SCREEN,
	CMD_STYLE,
	CMD_SPINBOX,
	CMD_SPINNER,
	CMD_SCALE,
	CMD_TEXTAREA,
	CMD_KEYBOARD,
    CMD_COUNT
} Command;


Command lookup_cmd(const char *s) {
    if (strcmp(s, "button") == 0)   return CMD_BUTTON;
	if (strcmp(s, "label") == 0)    return CMD_LABEL;
    if (strcmp(s, "arc") == 0)      return CMD_ARC;
    if (strcmp(s, "slider") == 0)   return CMD_SLIDER;
    if (strcmp(s, "led") == 0)      return CMD_LED;
    if (strcmp(s, "switch") == 0)   return CMD_SWITCH;
	if (strcmp(s, "bar") == 0)   	return CMD_BAR;
	if (strcmp(s, "tabview") == 0)  return CMD_TABVIEW;
	if (strcmp(s, "tileview") == 0) return CMD_TILEVIEW;
	if (strcmp(s, "list") == 0)     return CMD_LIST;
	if (strcmp(s, "roller") == 0)   return CMD_ROLLER;
	if (strcmp(s, "style") == 0)   return CMD_STYLE;
	if (strcmp(s, "spinbox") == 0)   return CMD_SPINBOX;
	if (strcmp(s, "spinner") == 0)   return CMD_SPINNER;
	if (strcmp(s, "screen") == 0)   return CMD_SCREEN;
	if (strcmp(s, "scale") == 0)   return CMD_SCALE;
	if (strcmp(s, "textarea") == 0)   return CMD_TEXTAREA;
	if (strcmp(s, "keyboard") == 0)   return CMD_KEYBOARD;
    return CMD_UNKNOWN;
}

/*
static OBJ primLVGLaddimg(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[1]);
	char* filename = obj2str(args[0]);
	const char *parent;
	if (argCount > 2) {
		parent = obj2str(args[2]);
	} else {
		parent = "lv_scr_act";
	}

	ui_add_image(obj_name,filename,parent);
	return falseObj;
}
*/

static OBJ primLVGLaddfont(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[1]);
	char* filename = obj2str(args[0]);
	
	ui_add_font(obj_name,filename);
	return falseObj;
}


static OBJ primLVGLaddimg(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[1]);
	char* filename = obj2str(args[0]);
	const char *parent;
	if (argCount > 2) {
		parent = obj2str(args[2]);
	} else {
		parent = "lv_scr_act";
	}

	ui_add_image(obj_name,filename,parent);
	return falseObj;
}

// primLVGL function definitions
static OBJ primLVGLprintall(int argCount, OBJ *args) {
	printall();
	return falseObj;
}

static OBJ primLVGLgetallobjs(int argCount, OBJ *args) {
	int count = registry.size();
	count += font_buffer.size();
	count += style_registry.size();
	count += series_registry.size();
	count += btnmap_registry.size();
	OBJ result = newObj(ListType, count+1, zeroObj);
	FIELD(result, 0) = int2obj(count);
	int i=1;
	std::vector<std::string> names = registry.getAllNames();
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	names = font_buffer.getAllNames();
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	names = style_registry.getAllNames();
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	names = series_registry.getAllNames();
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	names = btnmap_registry.getAllNames();
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	return result;
}


static OBJ primLVGLgetallfonts(int argCount, OBJ *args) {
	std::vector<std::string> names = font_buffer.getAllNames();
	int count = font_buffer.size();
	OBJ result = newObj(ListType, count+1, zeroObj);
	FIELD(result, 0) = int2obj(count);
	int i=1;
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	return result;
}


static OBJ primLVGLgetallstyles(int argCount, OBJ *args) {
	std::vector<std::string> names = style_registry.getAllNames();
	int count = style_registry.size();
	OBJ result = newObj(ListType, count+1, zeroObj);
	FIELD(result, 0) = int2obj(count);
	int i=1;
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	return result;
}


static OBJ primLVGLgetallseries(int argCount, OBJ *args) {
	std::vector<std::string> names = series_registry.getAllNames();
	int count = series_registry.size();
	OBJ result = newObj(ListType, count+1, zeroObj);
	FIELD(result, 0) = int2obj(count);
	int i=1;
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	return result;
}

static OBJ primLVGLgetallbtnmaps(int argCount, OBJ *args) {
	std::vector<std::string> names = btnmap_registry.getAllNames();
	int count = btnmap_registry.size();
	OBJ result = newObj(ListType, count+1, zeroObj);
	FIELD(result, 0) = int2obj(count);
	int i=1;
	for (const auto& name : names) {
		FIELD(result, i)=newStringFromBytes(name.c_str(), name.length());
		i++;
	}
	return result;
}

static OBJ primLVGLaddBtn(int argCount, OBJ *args) {
	int scale = 1;
	char* obj_name = obj2str(args[0]);
	const char *label_text;
	if (argCount >1) {
		scale = obj2int(args[1]);
	} else scale=1;
	if (argCount >2) {
		OBJ value = args[2];
		if (IS_TYPE(value, StringType)) {
			label_text = obj2str(value);
		} else if (isInt(value)) {
   			char s[20];
   			sprintf(s, "%d", obj2int(value));
			label_text=s;
		} else
			label_text="";
	} else 	label_text = obj2str(args[0]);
	const char *parent;
	if (argCount > 3) {
		parent = obj2str(args[3]);
	} else {
		parent = "lv_scr_act";
	}
	char s[100];
	sprintf(s,"btn: %s %s %d %s",obj_name, label_text, scale, parent );
	outputString(s);
	ui_create_button_label(obj_name, scale, label_text, parent);
	return falseObj;
}

static OBJ primLVGLaddLabel(int argCount, OBJ *args) {
	int scale = 1;
	char* label_name = obj2str(args[0]);
	const char *label_text;
	if (argCount >1) {
		scale = obj2int(args[1]);
	} else scale =1;
	if (argCount >2) {
		OBJ value = args[2];
		if (IS_TYPE(value, StringType)) {
			label_text = obj2str(args[2]);
		} else if (isInt(value)) {
   			char s[20];
   			sprintf(s, "%d", obj2int(value));
			label_text=s;
		} else
			label_text="";
	} else 	label_text = obj2str(args[0]);
	const char *parent;
	if (argCount > 3) {
		parent = obj2str(args[3]);
	} else {
		parent = "lv_scr_act";
	}
	ui_create_label(label_name, scale, label_text, parent);
	return falseObj;
}

static OBJ primLVGLaddSlider(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	const char *parent;
	if (argCount > 1) {
		parent = obj2str(args[1]);
	} else {
		parent = "lv_scr_act";
	}
	ui_create_slider(obj_name, parent);
	return falseObj;
}

static OBJ primLVGLaddTab(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	char* parent = obj2str(args[1]);
	ui_add_tab(obj_name, parent);
	return falseObj;
}

static OBJ primLVGLaddTile(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	char* parent = obj2str(args[1]);
	int col_id = obj2int(args[2]);
	int row_id = obj2int(args[3]);
	uint8_t l = (trueObj == args[4]) ? 1:0;
	uint8_t r = (trueObj == args[5]) ? 1:0;
	uint8_t t = (trueObj == args[6]) ? 1:0;
	uint8_t b = (trueObj == args[7]) ? 1:0;
	lv_dir_t dir = (lv_dir_t)(l + (r<<1) + (t<<2) + (b<<3));
	ui_add_tile(obj_name, parent, col_id, row_id, dir);
	return falseObj;
}

static OBJ primLVGLaddSeries(int argCount, OBJ *args) {
	char* series = obj2str(args[0]);
	char* chart = obj2str(args[1]);
	int color = obj2int(args[2]);
	ui_add_series(series, chart, color);
	return falseObj;
}

static OBJ primLVGLaddchart(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	char* chart_type = obj2str(args[1]);
	char* chart_update_mode = obj2str(args[2]);
	const char* parent = "lv_scr_act";
	if (argCount > 3) {
		parent = obj2str(args[3]);
	} 
	ui_add_chart(obj_name, parent, chart_type, chart_update_mode);
	return falseObj;
}

static OBJ primLVGLaddArc(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	const char *parent;
	if (argCount > 1) {
		parent = obj2str(args[1]);
	} else {
		parent = "lv_scr_act";
	}
	ui_create_arc(obj_name, parent);
	return falseObj;
}


void free_btnmap(char **btnmap, size_t count) {
    if (!btnmap) return;
    for (size_t i = 0; i <= count; i++) {
        free(btnmap[i]);  // free each string
    }
    free(btnmap);         // free the array of pointers
}




static OBJ primLVGLaddButtonMatrix(int argCount, OBJ *args) {
	int count;
	char* obj_name = obj2str(args[0]);
	const char *parent;
	OBJ obj = args[1];
	if (argCount > 2) {
		parent = obj2str(args[2]);
	} else {
		parent = "lv_scr_act";
	}

	if (IS_TYPE(obj, ListType)) {
		count = obj2int(FIELD(obj, 0));
		if (count >= WORDS(obj)) count = WORDS(obj) - 1;
		if (!registry.get(obj_name) && registry.get(parent)) {
			// alloc array of strings 
			char** btnmap = (char **)malloc((count+1) * sizeof(char*));
			for (size_t i = 1; i < count+1; i++) {
				OBJ field =  FIELD(obj, i);
				char* string_n = obj2str(field);
				size_t len = strlen(string_n);
				outputString(string_n);
				if (len==0) {
						btnmap[i-1] = (char *)malloc(2);
						strcpy(btnmap[i-1], "\n");
				} else {
					btnmap[i-1] = (char *)malloc(len + 1); // +1 for null terminator
					strcpy(btnmap[i-1], string_n);
				}
			}
			btnmap[count]=NULL; // end button map
			lv_obj_t* obj = lv_buttonmatrix_create(registry.get(parent));
			lv_buttonmatrix_set_map(obj, btnmap);
			lv_obj_add_event_cb(obj, ui_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
			registry.add(obj_name, obj);
			btnmap_registry.add(obj_name, btnmap); // store btnmap in registry
		}
	}
		
	return falseObj;
}



static OBJ primLVGLaddObject(int argCount, OBJ *args) {
	char* obj_type = obj2str(args[0]);
	char* obj_name = obj2str(args[1]);
	const char *parent;
	if (argCount > 2) {
		parent = obj2str(args[2]);
	} else {
		parent = "lv_scr_act";
	}
	//bool event = (argCount > 1) ? (trueObj == args[1]) : true;
	Command cmd = lookup_cmd(obj_type);
	switch (cmd) {
		case CMD_BUTTON:
			outputString("Handle BUTTON");
			ui_create_button(obj_name, parent);
			break;
		case CMD_ARC:
			outputString("Handle ARC");
			ui_create_arc(obj_name, parent);
			break;
		case CMD_SLIDER:
			outputString("Handle SLIDER");
			ui_create_slider(obj_name, parent);
			break;
		case CMD_LED:
			outputString("Handle LED");
			ui_create_led(obj_name, parent);
			break;
		case CMD_SWITCH:
			outputString("Handle SWITCH");
			ui_create_switch(obj_name, parent);
			break;
		case CMD_BAR:
			outputString("Handle BAR");
			ui_create_bar(obj_name, parent);
			break;
		case CMD_TABVIEW:
			outputString("Handle TABVIEW");
			ui_create_tabview(obj_name, parent);
			break;
		case CMD_TILEVIEW:
			outputString("Handle TILEVIEW");
			ui_create_tileview(obj_name, parent);
			break;
		case CMD_LIST:
			outputString("Handle LIST");
			ui_create_list(obj_name, parent);
			break;
		case CMD_ROLLER:
			outputString("Handle ROLLER");
			ui_create_roller(obj_name, parent);
			break;
		case CMD_SCREEN:
			outputString("Handle SCREEN");
			ui_create_screen(obj_name, parent);
			break;
		case CMD_STYLE:
		 	outputString("Handle STYLE");
		 	ui_create_style(obj_name, parent);
		 	break;
		case CMD_SPINBOX:
		 	outputString("Handle SPINBOX");
		 	ui_create_spinbox(obj_name, parent);
		 	break;
		case CMD_SPINNER:
		 	outputString("Handle SPINNER");
		 	ui_create_spinner(obj_name, parent);
		 	break;
		case CMD_SCALE:
		 	ui_create_scale(obj_name, parent);
		 	break;
		case CMD_TEXTAREA:
		 	ui_create_textarea(obj_name, parent);
		 	break;
		case CMD_KEYBOARD:
		 	ui_create_keyboard(obj_name, parent);
		 	break;
		default:
			outputString("Unknown command");;
	}
	return falseObj;
}

static OBJ primLVGLgetSymbol(int argCount, OBJ *args) {
    static const std::unordered_map<std::string, int> symbolMap = {
        {"bullet",        20042},
        {"audio",         61441},
        {"video",         61448},
        {"list",          61451},
        {"ok",            61452},
        {"close",         61453},
        {"power",         61457},
        {"settings",      61459},
        {"home",          61461},
        {"download",      61465},
        {"drive",         61468},
        {"refresh",       61473},
        {"mute",          61478},
        {"volume_mid",    61479},
        {"volume_max",    61480},
        {"image",         61502},
        {"tint",          61507},
        {"prev",          61512},
        {"play",          61515},
        {"pause",         61516},
        {"stop",          61517},
        {"next",          61521},
        {"eject",         61522},
        {"left",          61523},
        {"right",         61524},
        {"plus",          61543},
        {"minus",         61544},
        {"eye_open",      61550},
        {"eye_close",     61552},
        {"warning",       61553},
        {"shuffle",       61556},
        {"up",            61559},
        {"down",          61560},
        {"loop",          61561},
        {"directory",     61563},
        {"upload",        61587},
        {"call",          61589},
        {"cut",           61636},
        {"copy",          61637},
        {"save",          61639},
        {"bars",          61641},
        {"envelope",      61664},
        {"charge",        61671},
        {"paste",         61674},
        {"bell",          61683},
        {"keyboard",      61724},
        {"gps",           61732},
        {"file",          61787},
        {"wifi",          61931},
        {"battery_full",  62016},
        {"battery_3",     62017},
        {"battery_2",     62018},
        {"battery_1",     62019},
        {"battery_empty", 62020},
        {"usb",           62087},
        {"bluetooth",     62099},
        {"trash",         62189},
        {"edit",          62212},
        {"backspace",     62810},
        {"sd_card",       63426},
        {"new_line",      63650}
    };
	std::string symbol = obj2str(args[0]);
    auto it = symbolMap.find(symbol);
    if (it != symbolMap.end()) {
        return int2obj(it->second);
    }
    return falseObj; // not found
}

static OBJ primLVGLloadScreen(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	lv_obj_t* obj = registry.get(obj_name);
	//if (obj) lv_scr_load_anim(obj, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
	if (obj) lv_scr_load(obj);
	return falseObj;
}

static OBJ primLVGLsetParent(int argCount, OBJ *args) {
	char* obj = obj2str(args[0]);
	char* parent = obj2str(args[1]);
	// states and parts are only used when applying a style to an object]
	int states = 0;
	int parts = 0;
	if (argCount >2) {
		states = obj2int(args[2]);
	}
	if (argCount >3) {
		parts = obj2int(args[3]);
	}
	ui_set_parent(obj, parent, states, parts);
	return falseObj;
}

#if defined(COCUBE)
static OBJ primLVGLaddgroup(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	lv_obj_t* obj = registry.get(obj_name);
	lv_group_add_obj(group, obj);
	return falseObj;
}
#endif

static OBJ primLVGLdelObj(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	ui_delete_obj(obj_name);
	return falseObj;
}

static OBJ primLVGLsetSize(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	int w = obj2int(args[1]);
	int h = obj2int(args[2]);
	ui_set_size(obj_name, w, h);
	return falseObj;
}

static OBJ primLVGLsetPos(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	int pos_x = obj2int(args[1]);
	int pos_y = obj2int(args[2]);
	ui_set_pos(obj_name, pos_x, pos_y);
	return falseObj;
}

static OBJ primLVGLsetScroll(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	char* direction = obj2str(args[1]);
	lv_dir_t scroll_dir = LV_DIR_NONE;
	if (strcmp(direction,"hor")==0) scroll_dir = LV_DIR_HOR;
	else if (strcmp(direction,"ver")==0) scroll_dir = LV_DIR_VER;
	else if (strcmp(direction,"all")==0) scroll_dir = LV_DIR_ALL;
	ui_set_scroll(obj_name, scroll_dir);
	return falseObj;
}




static OBJ primLVGLsetVal(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	lv_obj_t* obj = registry.get(obj_name);
	int value;
	if (obj) {
		if (lv_obj_get_class(obj) == &lv_led_class) {
			if (trueObj == args[1]) lv_led_on(obj);
			else lv_led_off(obj);
		} else if (lv_obj_get_class(obj) == &lv_switch_class) {
			if (IS_TYPE(args[1], BooleanType )) {
				if (args[1]==trueObj) value = 1; else value=0;
			} 
		}
		else
		   value = obj2int(args[1]);
		if (lv_obj_get_class(obj) == &lv_arc_class) {
			lv_arc_set_value(obj, value);
		} else 
		if (lv_obj_get_class(obj) == &lv_slider_class) {
			lv_slider_set_value(obj, value, LV_ANIM_OFF);
		} else
		if (lv_obj_get_class(obj) == &lv_bar_class) {
			lv_bar_set_value(obj, value, LV_ANIM_OFF);
		} else
		if (lv_obj_get_class(obj) == &lv_spinbox_class) {
			lv_spinbox_set_value(obj, value);
		} else
		if (lv_obj_get_class(obj) == &lv_switch_class) {
			if (value==0) lv_obj_remove_state(obj, LV_STATE_CHECKED);
			else if (value>0) lv_obj_add_state(obj, LV_STATE_CHECKED);
		}  else
		if (lv_obj_get_class(obj) == &lv_roller_class) {
			lv_roller_set_visible_row_count(obj,value);
		} 
	}
	return falseObj;
}

static OBJ primLVGLsetnextvalue(int argCount, OBJ *args) {
	char* series = obj2str(args[0]);
	char *chart = obj2str(args[1]);
	int val = obj2int(args[2]);
	if (argCount > 3) {
		int val2 = obj2int(args[3]);
		ui_set_next_value2(series, chart, val, val2);
	} else {
		ui_set_next_value(series, chart, val);
	}
		return falseObj;
}	


static OBJ primLVGLsetText(int argCount, OBJ *args) {
	int scale = 1;
	char* obj_name = obj2str(args[0]);
	char* obj_text;
	char *font_name; 
	OBJ value = args[1];
	if (IS_TYPE(value, StringType)) {
		obj_text = obj2str(value);
	} else if (isInt(value)) {
		char s[20];
		sprintf(s, "%d", obj2int(value));
		obj_text = s;
	} else {
		char s[1];
		s[0]='\0';
		obj_text=s;
	}
	if (argCount >2) {
		if (IS_TYPE(args[2], StringType)) {
			font_name = obj2str(args[2]);
			ui_set_text_font(obj_name, obj_text, font_name);
		} else {
			scale = obj2int(args[2]);
			ui_set_text(obj_name, obj_text, scale);
		}
	} else 
		ui_set_text(obj_name, obj_text, scale);
	
	return falseObj;
}

static OBJ primLVGLsetattribute(int argCount, OBJ *args) {
	char* attribute_name = obj2str(args[0]);
	char* obj_name = obj2str(args[1]);
	int to_val = obj2int(args[2]);
	int until_val=100;	
	if (argCount >3) {
		until_val = obj2int(args[3]);
	}
	char s[100];
	sprintf(s,"set attritube to %d until %d",to_val, until_val);
	outputString(s);
	ui_set_attribute(obj_name, attribute_name, to_val, until_val);
	return falseObj;
}


static OBJ primLVGLsetstyle(int argCount, OBJ *args) {
	char* style_name = obj2str(args[0]);
	char* obj_name = obj2str(args[1]);
	int to_val = obj2int(args[2]);
	ui_set_style(obj_name, style_name, to_val);
	return falseObj;
}

static OBJ primLVGLgetVal(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	lv_obj_t* obj = registry.get(obj_name);
	if (obj) {
		if (lv_obj_get_class(obj) == &lv_arc_class) {
			return int2obj(lv_arc_get_value(obj));
 		} else 
		if (lv_obj_get_class(obj) == &lv_slider_class) {
			return int2obj(lv_slider_get_value(obj));
		} else 
		if (lv_obj_get_class(obj) == &lv_spinbox_class) {
			return int2obj(lv_spinbox_get_value(obj));
		} else 
		if (lv_obj_get_class(obj) == &lv_textarea_class) {
			const char * text = lv_textarea_get_text(obj);
			return newStringFromBytes(text, strlen(text));
		} else 
		if (lv_obj_get_class(obj) == &lv_switch_class) {
			return lv_obj_has_state(obj, LV_STATE_CHECKED)  ? trueObj : falseObj;
		} else 
		if (lv_obj_get_class(obj) == &lv_roller_class) {
			char buf[100];
			lv_roller_get_selected_str(obj, buf, sizeof(buf));
			return  newStringFromBytes(buf, strlen(buf));
		} else
		if (lv_obj_check_type(obj, &lv_buttonmatrix_class)){
			// used when event to retuen the id of the btn
			char s[100];
			sprintf(s,"btn: %s id: %d",obj_name, lv_buttonmatrix_get_selected_button(obj) );
			outputString(s);
			int id = lv_buttonmatrix_get_selected_button(obj);
			int checked = 0;
			if (lv_buttonmatrix_has_button_ctrl(obj, id, LV_BTNMATRIX_CTRL_CHECKED)) checked = 512;
			return int2obj(id + checked);
		} 
		else return falseObj;
	} else return falseObj;
}




static OBJ primLVGLsetColor(int argCount, OBJ *args) {
	char* obj_name = obj2str(args[0]);
	int color = obj2int(args[1]);
	ui_set_color(obj_name, color);
	if ( argCount > 2) {
		color = obj2int(args[2]);
		ui_set_color_2nd(obj_name, color);
	}
	if ( argCount > 3) {
		color = obj2int(args[3]);
		ui_set_color_3rd(obj_name, color);
	}
	
	return falseObj;
}


static OBJ primLVGLgetEvent(int argCount, OBJ *args) {
	std::string name;
	int code = ui_get_last_event(name);
	char s[100];
	sprintf(s,"event: %s code: %d",name.c_str(),code);
	outputString(s);	
	OBJ result = newStringFromBytes(name.c_str(), name.length());
	return result;
}

static OBJ primLVGLEvent(int argCount, OBJ *args) {
	bool check_event = event_seen;
	// char s[100];
	// sprintf(s,"event %d",event_seen);
	// outputString(s);
	event_seen = false; // wipe event for next event
	if (check_event) return trueObj; else return falseObj;
}

 
static OBJ primLVGLon(int argCount, OBJ *args) {
	set_lvgl(trueObj == args[0]);
	return falseObj;

}

static OBJ primLVGLtick(int argCount, OBJ *args) {
	lvgl_tick();
	return falseObj;

}

// dummy function for testing initialisation lvgl
static OBJ primLVGLinit(int argCount, OBJ *args) {
	outputString("Before setup_lvgl");
	setup_lvgl();
	outputString("After setup_lvgl");
	return falseObj;

}

static OBJ primLVGLstate(int argCount, OBJ *args) {
	char s[100];
	sprintf(s,"uselvgl: %d, lvgl_initialzed: %d\n", useLVGL, LVGL_initialized);
	outputString(s);	
	return falseObj;

}

#include "esp_heap_caps.h"

static OBJ primLVGLpsram(int argCount, OBJ *args) {
	int val = heap_caps_get_free_size(MALLOC_CAP_SPIRAM); // in bytes

	char s[100];
	sprintf(s,"PSRAM total size: %d",ESP.getPsramSize());
	outputString(s);	
	sprintf(s,"PSRAM free: %d",val);
    outputString(s);	
	sprintf(s,"PSRAM largest free block: %d",heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	outputString(s);	
	sprintf(s,"RAM heap free: %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    outputString(s);	
	sprintf(s,"free heap after: %d psram: %d ",  ESP.getFreeHeap(),ESP.getFreePsram());
	outputString(s);
	return int2obj(val);
}

#if defined(LVGL_SNAPSHOT)
#include <WiFi.h>
#include <HTTPClient.h>


#define STRING_OBJ_CONST(s) \
	struct { uint32 header = HEADER(StringType, ((sizeof(s) + 4) / 4)); char body[sizeof(s)] = s; }


STRING_OBJ_CONST("Snapshot failed") statusSnapshotFailed;
STRING_OBJ_CONST("PSRAM alloc failed") statusPsramAllocFailed;



static OBJ primLVGLsnapshot(int argCount, OBJ *args) {
	char *upload_url = obj2str(args[0]);
 	lv_obj_t *scr = lv_screen_active();
    lv_coord_t w = lv_obj_get_width(scr);
    lv_coord_t h = lv_obj_get_height(scr);
    size_t buf_size = w * h * 2; // RGB565 = 2 bytes per pixel

    // Allocate snapshot buffer in PSRAM
    uint8_t *psram_buf = (uint8_t *) heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!psram_buf) {
		return (OBJ)  &statusPsramAllocFailed;
    }

    // Create LVGL image descriptor
    lv_image_dsc_t snapshot;
    lv_result_t res = lv_snapshot_take_to_buf(scr,
                                              LV_COLOR_FORMAT_NATIVE,
                                              &snapshot,
                                              psram_buf,
                                              buf_size);
    if (res != LV_RESULT_OK) {
        lv_free(psram_buf);
        return (OBJ) &statusSnapshotFailed;
    }
	char s[100];
	sprintf(s,"📸 Snapshot OK: %d x %d (%d bytes)\n", w, h, buf_size);
	outputString(s);	
    
    // --- Upload to Web Server ---
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(upload_url);
        http.addHeader("Content-Type", "application/octet-stream");

        int httpResponseCode = http.POST(psram_buf, buf_size);
        if (httpResponseCode > 0) {
            sprintf(s,"✅ Upload OK, code: %d\n", httpResponseCode);
			outputString(s);	
        } else {
			sprintf(s,"❌ Upload failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
			outputString(s);			
        }
        http.end();
    } else {
         fail(noWiFi);
    }

    // Cleanup
    free(psram_buf);
  	return falseObj;

}



#endif   // LVGL_SNAPSHOT

#endif 
// Touchscreen Primitives

static OBJ primTftTouched(int argCount, OBJ *args) {
	#ifdef HAS_TOUCH_SCREEN
		return screenTouched() ? trueObj : falseObj;
	#endif
	return falseObj;
}

static OBJ primTftTouchX(int argCount, OBJ *args) {
	#ifdef HAS_TOUCH_SCREEN
		return int2obj(screenTouchX());
	#endif
	return int2obj(-1);
}

static OBJ primTftTouchY(int argCount, OBJ *args) {
	#ifdef HAS_TOUCH_SCREEN
		return int2obj(screenTouchY());
	#endif
	return int2obj(-1);
}

#if defined(LMS7789)
static OBJ primTftTouchGesture(int argCount, OBJ *args) {
	#ifdef HAS_TOUCH_SCREEN
		return int2obj(screenTouchGesture());
	#endif
	return int2obj(-1);
}
#endif
static OBJ primTftTouchPressure(int argCount, OBJ *args) {
	#ifdef HAS_TOUCH_SCREEN
		return int2obj(screenTouchPressure());
	#endif
	return int2obj(-1);
}



// Primitives

static PrimEntry entries[] = {
	{"setBacklight", primSetBacklight},
	{"getWidth", primGetWidth},
	{"getHeight", primGetHeight},
	{"setPixel", primSetPixel},
	{"pixelRow", primPixelRow},
	{"line", primLine},
	{"rect", primRect},
	{"roundedRect", primRoundedRect},
	{"circle", primCircle},
	{"triangle", primTriangle},
	{"text", primText},
	{"clear", primClear},
	{"deferUpdates", primDeferUpdates},
	{"resumeUpdates", primResumeUpdates},

	{"mergeBitmap", primMergeBitmap},
	{"drawBuffer", primDrawBuffer},
	{"drawBitmap", primDrawBitmap},

	{"tftTouched", primTftTouched},
	{"tftTouchX", primTftTouchX},
	{"tftTouchY", primTftTouchY},
#if defined(LMS7789)
	{"tftTouchGesture", primTftTouchGesture},
#endif	
	{"tftTouchPressure", primTftTouchPressure},

	{"aruco", primAruco},
	{"aprilTag", primAprilTag},
#if defined(LVGL) 
	{"LVGLon",primLVGLon},
//	{"LVGLbutton",primLVGLbutton},
	{"LVGLtick",primLVGLtick},
	{"LVGLstate",primLVGLstate},
	#if defined(LVGL_SNAPSHOT)
		{"LVGLsnapshot",primLVGLsnapshot},
	#endif
	{"LVGLaddbtn",primLVGLaddBtn},
	{"LVGLaddlabel",primLVGLaddLabel},
	{"LVGLaddslider",primLVGLaddSlider},
	{"LVGLaddarc",primLVGLaddArc},
	{"LVGLaddtab",primLVGLaddTab},
	{"LVGLaddseries",primLVGLaddSeries},
	{"LVGLaddchart",primLVGLaddchart},
	{"LVGLsetnextvalue",primLVGLsetnextvalue},
	{"LVGLaddtile",primLVGLaddTile},
	{"LVGLaddbuttonmatrix",primLVGLaddButtonMatrix},
	{"LVGLaddobj",primLVGLaddObject},
	{"LVGLdelobj",primLVGLdelObj},
	{"LVGLsetparent",primLVGLsetParent},
	{"LVGLsetpos",primLVGLsetPos},
	{"LVGLsetsize",primLVGLsetSize},
	{"LVGLsetval", primLVGLsetVal},
	{"LVGLsettext",primLVGLsetText},
	{"LVGLsetattribute",primLVGLsetattribute},
	{"LVGLsetstyle",primLVGLsetstyle},
	{"LVGLgetval", primLVGLgetVal},
	{"LVGLloadscreen",primLVGLloadScreen},
	{"LVGLevent",primLVGLEvent},
	{"LVGLgetevent",primLVGLgetEvent},
	{"LVGLsetcolor", primLVGLsetColor},
	{"LVGLgetallobjs", primLVGLgetallobjs},
	{"LVGLgetallfonts",primLVGLgetallfonts},
	{"LVGLgetallstyles",primLVGLgetallstyles},
	{"LVGLgetallseries",primLVGLgetallseries},
	{"LVGLgetallbtnmaps",primLVGLgetallbtnmaps},
	{"LVGLgetsymbol",primLVGLgetSymbol},
	{"LVGLinit", primLVGLinit},
	{"LVGLaddimg", primLVGLaddimg},
 	{"LVGLaddfont",primLVGLaddfont},
	{"LVGLsetscroll",primLVGLsetScroll},
	{"LVGLpsram",primLVGLpsram},
	#if (defined(LMSDIAPLY) && defined(BREAKOUT))||defined(CYDROT)
		{"fliptouch",primfliptouch},
	#endif
	#if defined(COCUBE)
		{"LVGLaddgroup",primLVGLaddgroup},
	#endif

#endif
};

void addTFTPrims() {
	addPrimitiveSet(TFTPrims, "tft", sizeof(entries) / sizeof(PrimEntry), entries);
}