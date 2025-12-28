/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2019 John Maloney, Bernat Romagosa, and Jens Mönig

// miscPrims.c - Miscellaneous primitives
// John Maloney, May 2019

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "interp.h"
#include "tinyJSON.h"
#include "version.h"

OBJ primVersion(int argCount, OBJ *args) {
	int result = atoi(&VM_VERSION[1]); // skip initial "v"
	return int2obj(result);
}

OBJ primBLE_ID(int argCount, OBJ *args) {
	OBJ result;
	if (strlen(BLE_ThreeLetterID) == 3) {
		result = newStringFromBytes(BLE_ThreeLetterID, 3);
	} else {
		const char bleNotSupported[] = "BLE not supported";
		result = newStringFromBytes(bleNotSupported, strlen(bleNotSupported));
	}
	if (!result) return fail(insufficientMemoryError);
	return result;
}

OBJ primHexToInt(int argCount, OBJ *args) {
	if (!IS_TYPE(args[0], StringType)) return fail(needsStringError);

	char *s = obj2str(args[0]);
	if ('#' == *s) s++; // skip leading # if there is one
	if (('0' == *s) && (('x' == s[1]) || ('X' == s[1]))) s += 2; // skip leading '0x' or '0X'
	long result = strtol(s, NULL, 16);
	result = (result << 1) >> 1; // extend sign bit if bit 31 is set
	if ((result < -0x40000000) || (result > 0x3FFFFFFF)) return fail(hexRangeError);
	return int2obj(result);
}

OBJ primBinaryToInt(int argCount, OBJ *args) {
	if (!IS_TYPE(args[0], StringType)) return fail(needsStringError);

	char *s = obj2str(args[0]);
	long result = strtol(s, NULL, 2);
	result = (result << 1) >> 1; // extend sign bit if bit 31 is set
	if ((result < -0x40000000) || (result > 0x3FFFFFFF)) return fail(hexRangeError);
	return int2obj(result);
}

OBJ primRescale(int argCount, OBJ *args) {
	if (argCount < 5) return fail(notEnoughArguments);
	int inVal = evalInt(args[0]);
	int inMin = evalInt(args[1]);
	int inMax = evalInt(args[2]);
	int outMin = evalInt(args[3]);
	int outMax = evalInt(args[4]);

	if (inMax == inMin) return fail(zeroDivide);

	int result = outMin + (((inVal - inMin) * (outMax - outMin)) / (inMax - inMin));
	return int2obj(result);
}

// HSV Colors

static float clampedPercent(int percent) {
	// Return a float between 0.0 and 1.0 for the given percent.

	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	return (float) percent / 100.0;
}

static void extractHSV(int rgb, float *hue, float *sat, float *bri) {
	int r = (rgb >> 16) & 255;
	int g = (rgb >> 8) & 255;
	int b = rgb & 255;

	int min = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
	int max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);

	if (max == min) {
		// gray; hue is arbitrarily chosen to be zero
		*hue = 0.0;
		*sat = 0.0;
		*bri = max / 255.0;
	}

	int f = 0;
	int i = 0;
	if (r == min) {
		f = g - b;
		i = 3;
	} else if(g == min) {
		f = b - r;
		i = 5;
	} else if (b == min) {
		f = r - g;
		i = 1;
	}

	*hue = fmod(60.0 * (i - (((float) f) / (max - min))), 360.0);
	*sat = 0.0;
	if (max > 0) *sat = ((float) (max - min)) / max;
	*bri = max / 255.0;
}

OBJ primHSVColor(int argCount, OBJ *args) {
	if (argCount < 3) return fail(notEnoughArguments);

	int h = evalInt(args[0]) % 360;
	if (h < 0) h += 360;
	float s = clampedPercent(evalInt(args[1]));
	float v = clampedPercent(evalInt(args[2]));

	int i = h / 60;
	float f = (h / 60.0) - i;
	float p = v * (1.0 - s);
	float q = v * (1.0 - (s * f));
	float t = v * (1.0 - (s * (1.0 - f)));
	float r, g, b;

	switch (i) {
	case 0:
		r = v; g = t; b = p;
		break;
	case 1:
		r = q; g = v; b = p;
		break;
	case 2:
		r = p; g = v; b = t;
		break;
	case 3:
		r = p; g = q; b = v;
		break;
	case 4:
		r = t; g = p; b = v;
		break;
	case 5:
		r = v; g = p; b = q;
		break;
	}

	int rgb = (((int) (255 * r)) << 16) | (((int) (255 * g)) << 8) | ((int) (255 * b));
	return int2obj(rgb);
}

OBJ primColorHue(int argCount, OBJ *args) {
	if (argCount < 1) return fail(notEnoughArguments);
	float h, s, v;
	extractHSV(evalInt(args[0]), &h, &s, &v);
	return int2obj((int) h);
}

OBJ primColorSaturation(int argCount, OBJ *args) {
	if (argCount < 1) return fail(notEnoughArguments);
	float h, s, v;
	extractHSV(evalInt(args[0]), &h, &s, &v);
	return int2obj((int) (100.0 * s));
}

OBJ primColorBrightness(int argCount, OBJ *args) {
	if (argCount < 1) return fail(notEnoughArguments);
	float h, s, v;
	extractHSV(evalInt(args[0]), &h, &s, &v);
	return int2obj((int) (100.0 * v));
}

static int16 sineTable[91] = {
	0, 286, 572, 857, 1143, 1428, 1713, 1997, 2280, 2563, 2845,
	3126, 3406, 3686, 3964, 4240, 4516, 4790, 5063, 5334, 5604,
	5872, 6138, 6402, 6664, 6924, 7182, 7438, 7692, 7943, 8192,
	8438, 8682, 8923, 9162, 9397, 9630, 9860, 10087, 10311, 10531,
	10749, 10963, 11174, 11381, 11585, 11786, 11982, 12176, 12365, 12551,
	12733, 12911, 13085, 13255, 13421, 13583, 13741, 13894, 14044, 14189,
	14330, 14466, 14598, 14726, 14849, 14968, 15082, 15191, 15296, 15396,
	15491, 15582, 15668, 15749, 15826, 15897, 15964, 16026, 16083, 16135,
	16182, 16225, 16262, 16294, 16322, 16344, 16362, 16374, 16382, 16384};

static OBJ primIntSine(int argCount, OBJ *args) {
	// Returns the sine of the given angle * 2^14 (i.e. a fixed point integer with 13 bits of
	// fraction). The input is the angle in hundreths of a degree (e.g. 4500 means 45 degrees).
	// This version uses table lookup with interpolation and uses int integer math, no floats.

	// This version using floats is 3x to 4x slower:
	//	const float hundrethsToRadians = 6.2831853071795864769 / 36000.0;
	//	return int2obj((int) round(16384.0 * sin(evalInt(args[0]) * hundrethsToRadians)));

	int angle = evalInt(args[0]) % 36000;
	if (angle < 0) angle += 36000; // positive angle in hundreds of a degree [0..35999]

	int sign = 1;
	if (angle < 9000) {
		// first quarter; use angle directly
	} else if (angle < 18000) {
		angle = 18000 - angle; // second quarter: reverse of first quarter
	} else if (angle < 27000) {
		sign = -1;
		angle = angle - 18000; // third quarter; like first quarter but invert sign of output
	} else {
		sign = -1;
		angle = 36000 - angle; // fourth quarter; like second quarter but invert sign of output
	}

	int i = angle / 100; // sineTable index
	int frac = angle % 100; // fraction (0-99)

	int result = sineTable[i];
	if (frac) {
		result = (((100 - frac) * result) + (frac * sineTable[i + 1])) / 100;
	}

	return int2obj(sign * result);
}

static OBJ primIntSqrt(int argCount, OBJ *args) {
	// Returns the integer square root of a given number rounded to the nearest integer.
	// For example, sqrt(9) = 3. To get more precision, you can pre-multiply by a scaling
	// factor squared. For example, to get two digits of precision you can multiple by
	// 100 * 100 = 10000. The square root of two with two digits: sqrt(20000) = 141

	int n = evalInt(args[0]);
	if (n < 0) n = -n; // xxx should we give an error here?

	// The following code generates same values as:
	//		round(sqrt(n)))
	// without using floats. It uses Heron's method, a special case of Newton's method.
	// https://en.wikipedia.org/wiki/Integer_square_root

	if (n < 2) return int2obj(n); // 0 and 1 return themselves

	int x0 = n / 2; // initial estimate (must be greater than the square root
	int x1 = (x0 + (n / x0)) / 2;
	while (x0 > x1) {
		x0 = x1;
		x1 = (x0 + (n / x0)) / 2;
	}

	// choose the closer square root between x0 and x0 + 1
	int next = x0 + 1;
	if (((next * next) - n) < (n - (x0 * x0))) {
		x0 = next;
	}
	return int2obj(x0);
}

static OBJ primArctan(int argCount, OBJ *args) {
	// Returns angle (in hundredths of a degree) of vector dx, dy.

	if (argCount < 2) return fail(notEnoughArguments);
	if (!isInt(args[0]) || !isInt(args[1])) return fail(needsIntegerError);

	double x = obj2int(args[0]);
	double y = obj2int(args[1]);
	double degreeHundredths = (18000.0 * atan2(y, x)) / 3.141592653589793238463;

	return int2obj((int) round(degreeHundredths));
}

static OBJ primPressureToAltitude(int argCount, OBJ *args) {
	// Computes the altitude difference (in millimeters) for a given pressure difference.
	// dH = 44330 * [ 1 - ( p / p0 ) ^ ( 1 / 5.255) ]

	if (argCount < 2) return fail(notEnoughArguments);
	int p0 = obj2int(args[0]);
	int p = obj2int(args[1]);
	double result = 44330.0 * (1.0 - pow((double) p / p0, (1.0 / 5.255))); // meters
	return int2obj((int) (1000.0 * result)); // return result in millimeters
}

static OBJ primConnectedToIDE(int argCount, OBJ *args) {
	return ideConnected() ? trueObj : falseObj;
}

static OBJ primScriptTooLarge(int argCount, OBJ *args) {
	// Used by IDE to report scriptTooLarge errors.

	return fail(scriptTooLarge);
}

static OBJ jsonValue(char *item) {
	char buf[1024];
	char *end;
	if (!item) return newString(0); // path not found

	switch (tjr_type(item)) {
	case tjr_Array:
	case tjr_Object:
		end = tjr_endOfItem(item);
		return newStringFromBytes(item, (end - item));
	case tjr_Number:
		return int2obj(tjr_readInteger(item));
	case tjr_String:
		tjr_readStringInto(item, buf, sizeof(buf));
		return newStringFromBytes(buf, strlen(buf));
	case tjr_True:
		return trueObj;
	case tjr_False:
		return falseObj;
	case tjr_Null:
		return newStringFromBytes("null", 4);
	}
	return newString(0); // json parse error or end
}

static OBJ primJSONGet(int argCount, OBJ *args) {
	// Return the value at the given path in a JSON string or the empty string
	// if the path doesn't refer to anything. The optional third argument returns
	// the value of the Nth element of an array or object.

	if (argCount < 2) return fail(notEnoughArguments);
	if (!IS_TYPE(args[0], StringType)) return fail(needsStringError);
	if (!IS_TYPE(args[1], StringType)) return fail(needsStringError);
	char *json = obj2str(args[0]);
	char *path = obj2str(args[1]);
	int i = ((argCount > 2) && isInt(args[2])) ? obj2int(args[2]) : -1;

	char *item = tjr_atPath(json, path);
	int itemType = tjr_type(item);
	if ((tjr_Array == itemType) && (i > 0)) {
		item++; // skip '['
		for (; i > 1; i--) item = tjr_nextElement(item);
	}
	if ((tjr_Object == itemType) && (i > 0)) {
		item++; // skip '{'
		for (; i > 1; i--) {
			item = tjr_nextProperty(item, NULL, 0);
			item = tjr_nextElement(item); // skip value
		}
	}
	return jsonValue(item);
}

static OBJ primJSONCount(int argCount, OBJ *args) {
	// Return the number of entries in the array or entry at the given path of a JSON string.

	if (argCount < 2) return fail(notEnoughArguments);
	if (!IS_TYPE(args[0], StringType)) return fail(needsStringError);
	if (!IS_TYPE(args[1], StringType)) return fail(needsStringError);
	char *json = obj2str(args[0]);
	char *path = obj2str(args[1]);

	char *item = tjr_atPath(json, path);
	return int2obj(tjr_count(item));
}

static OBJ primJSONValueAt(int argCount, OBJ *args) {
	// Return the value for the Nth object or array entry at the given path of a JSON string.

	if (argCount < 3) return fail(notEnoughArguments);
	if (!IS_TYPE(args[0], StringType)) return fail(needsStringError);
	if (!IS_TYPE(args[1], StringType)) return fail(needsStringError);
	if (!isInt(args[2])) return fail(needsIntegerError);
	char *json = obj2str(args[0]);
	char *path = obj2str(args[1]);
	int i = obj2int(args[2]);

	char *item = tjr_atPath(json, path);
	return jsonValue(tjr_valueAt(item, i));
}

static OBJ primJSONKeyAt(int argCount, OBJ *args) {
	// Return the key for the Nth object entry at the given path of a JSON string.

	if (argCount < 3) return fail(notEnoughArguments);
	if (!IS_TYPE(args[0], StringType)) return fail(needsStringError);
	if (!IS_TYPE(args[1], StringType)) return fail(needsStringError);
	if (!isInt(args[2])) return fail(needsIntegerError);
	char *json = obj2str(args[0]);
	char *path = obj2str(args[1]);
	int i = obj2int(args[2]);

	char key[100];
	key[0] = '\0';
	char *item = tjr_atPath(json, path);
	tjr_keyAt(item, i, key, sizeof(key));
	return newStringFromBytes(key, strlen(key));
}

static OBJ primBMP680GasResistance(int argCount, OBJ *args) {
	if (argCount < 3) return fail(notEnoughArguments);
	int gas_res_adc = evalInt(args[0]);
	int gas_range = evalInt(args[1]);
	int range_sw_err = evalInt(args[2]);

	// ensure that gas_range is [0..15]
	if (gas_range < 0) gas_range = 0;
	if (gas_range > 15) gas_range = 15;

	/* Look up table 1 for the possible gas range values */
	uint32_t lookupTable1[16] = {
		UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647),
		UINT32_C(2147483647), UINT32_C(2126008810), UINT32_C(2147483647), UINT32_C(2130303777),
		UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2143188679), UINT32_C(2136746228),
		UINT32_C(2147483647), UINT32_C(2126008810), UINT32_C(2147483647), UINT32_C(2147483647) };

	/* Look up table 2 for the possible gas range values */
	uint32_t lookupTable2[16] = {
		UINT32_C(4096000000), UINT32_C(2048000000), UINT32_C(1024000000), UINT32_C(512000000),
		UINT32_C(255744255), UINT32_C(127110228), UINT32_C(64000000), UINT32_C(32258064), UINT32_C(16016016),
		UINT32_C(8000000), UINT32_C(4000000), UINT32_C(2000000), UINT32_C(1000000), UINT32_C(500000),
		UINT32_C(250000), UINT32_C(125000) };

	int64_t var1 = (int64_t) ((1340 + (5 * (int64_t) range_sw_err)) *
		((int64_t) lookupTable1[gas_range])) >> 16;
	uint64_t var2 = (((int64_t) ((int64_t) gas_res_adc << 15) - (int64_t) (16777216)) + var1);
	int64_t var3 = (((int64_t) lookupTable2[gas_range] * (int64_t) var1) >> 9);
	uint32_t calc_gas_res = (uint32_t) ((var3 + ((int64_t) var2 >> 1)) / (int64_t) var2);

	return int2obj(calc_gas_res);
}

const uint8 font5x7[95 * 5] = {
	0x00, 0x00, 0x00, 0x00, 0x00,	// space
	0x00, 0x00, 0x4f, 0x00, 0x00,	// !
	0x00, 0x07, 0x00, 0x07, 0x00,	// "
	0x14, 0x7f, 0x14, 0x7f, 0x14,	// #
	0x24, 0x2a, 0x7f, 0x2a, 0x12,	// $
	0x23, 0x13, 0x08, 0x64, 0x62,	// %
	0x36, 0x49, 0x55, 0x22, 0x20,	// &
	0x00, 0x05, 0x03, 0x00, 0x00,	// '
	0x00, 0x1c, 0x22, 0x41, 0x00,	// (
	0x00, 0x41, 0x22, 0x1c, 0x00,	// )
	0x14, 0x08, 0x3e, 0x08, 0x14,	// //
	0x08, 0x08, 0x3e, 0x08, 0x08,	// +
	0x50, 0x30, 0x00, 0x00, 0x00,	// ,
	0x08, 0x08, 0x08, 0x08, 0x08,	// -
	0x00, 0x60, 0x60, 0x00, 0x00,	// .
	0x20, 0x10, 0x08, 0x04, 0x02,	// /
	0x3e, 0x51, 0x49, 0x45, 0x3e,	// 0
	0x00, 0x42, 0x7f, 0x40, 0x00,	// 1
	0x42, 0x61, 0x51, 0x49, 0x46,	// 2
	0x21, 0x41, 0x45, 0x4b, 0x31,	// 3
	0x18, 0x14, 0x12, 0x7f, 0x10,	// 4
	0x27, 0x45, 0x45, 0x45, 0x39,	// 5
	0x3c, 0x4a, 0x49, 0x49, 0x30,	// 6
	0x01, 0x71, 0x09, 0x05, 0x03,	// 7
	0x36, 0x49, 0x49, 0x49, 0x36,	// 8
	0x06, 0x49, 0x49, 0x29, 0x1e,	// 9
	0x00, 0x36, 0x36, 0x00, 0x00,	// :
	0x00, 0x56, 0x36, 0x00, 0x00,	// ;
	0x08, 0x14, 0x22, 0x41, 0x00,	// <
	0x14, 0x14, 0x14, 0x14, 0x14,	// =
	0x00, 0x41, 0x22, 0x14, 0x08,	// >
	0x02, 0x01, 0x51, 0x09, 0x06,	// ?
	0x3e, 0x41, 0x5d, 0x55, 0x1e,	// @
	0x7e, 0x11, 0x11, 0x11, 0x7e,	// A
	0x7f, 0x49, 0x49, 0x49, 0x36,	// B
	0x3e, 0x41, 0x41, 0x41, 0x22,	// C
	0x7f, 0x41, 0x41, 0x22, 0x1c,	// D
	0x7f, 0x49, 0x49, 0x49, 0x41,	// E
	0x7f, 0x09, 0x09, 0x09, 0x01,	// F
	0x3e, 0x41, 0x49, 0x49, 0x7a,	// G
	0x7f, 0x08, 0x08, 0x08, 0x7f,	// H
	0x00, 0x41, 0x7f, 0x41, 0x00,	// I
	0x20, 0x40, 0x41, 0x3f, 0x01,	// J
	0x7f, 0x08, 0x14, 0x22, 0x41,	// K
	0x7f, 0x40, 0x40, 0x40, 0x40,	// L
	0x7f, 0x02, 0x0c, 0x02, 0x7f,	// M
	0x7f, 0x04, 0x08, 0x10, 0x7f,	// N
	0x3e, 0x41, 0x41, 0x41, 0x3e,	// O
	0x7f, 0x09, 0x09, 0x09, 0x06,	// P
	0x3e, 0x41, 0x51, 0x21, 0x5e,	// Q
	0x7f, 0x09, 0x19, 0x29, 0x46,	// R
	0x26, 0x49, 0x49, 0x49, 0x32,	// S
	0x01, 0x01, 0x7f, 0x01, 0x01,	// T
	0x3f, 0x40, 0x40, 0x40, 0x3f,	// U
	0x1f, 0x20, 0x40, 0x20, 0x1f,	// V
	0x3f, 0x40, 0x38, 0x40, 0x3f,	// W
	0x63, 0x14, 0x08, 0x14, 0x63,	// X
	0x07, 0x08, 0x70, 0x08, 0x07,	// Y
	0x61, 0x51, 0x49, 0x45, 0x43,	// Z
	0x00, 0x7f, 0x41, 0x41, 0x00,	// [
	0x02, 0x04, 0x08, 0x10, 0x20,	// (backslash)
	0x00, 0x41, 0x41, 0x7f, 0x00,	// ]
	0x04, 0x02, 0x01, 0x02, 0x04,	// ^
	0x40, 0x40, 0x40, 0x40, 0x40,	// _
	0x00, 0x00, 0x03, 0x05, 0x00,	// `
	0x20, 0x54, 0x54, 0x54, 0x78,	// a
	0x7F, 0x44, 0x44, 0x44, 0x38,	// b
	0x38, 0x44, 0x44, 0x44, 0x44,	// c
	0x38, 0x44, 0x44, 0x44, 0x7f,	// d
	0x38, 0x54, 0x54, 0x54, 0x18,	// e
	0x04, 0x04, 0x7e, 0x05, 0x05,	// f
	0x08, 0x54, 0x54, 0x54, 0x3c,	// g
	0x7f, 0x08, 0x04, 0x04, 0x78,	// h
	0x00, 0x44, 0x7d, 0x40, 0x00,	// i
	0x20, 0x40, 0x44, 0x3d, 0x00,	// j
	0x7f, 0x10, 0x28, 0x44, 0x00,	// k
	0x00, 0x41, 0x7f, 0x40, 0x00,	// l
	0x7c, 0x04, 0x7c, 0x04, 0x78,	// m
	0x7c, 0x08, 0x04, 0x04, 0x78,	// n
	0x38, 0x44, 0x44, 0x44, 0x38,	// o
	0x7c, 0x14, 0x14, 0x14, 0x08,	// p
	0x08, 0x14, 0x14, 0x14, 0x7c,	// q
	0x7c, 0x08, 0x04, 0x04, 0x08,	// r
	0x48, 0x54, 0x54, 0x54, 0x24,	// s
	0x04, 0x04, 0x3f, 0x44, 0x44,	// t
	0x3c, 0x40, 0x40, 0x20, 0x7c,	// u
	0x1c, 0x20, 0x40, 0x20, 0x1c,	// v
	0x3c, 0x40, 0x30, 0x40, 0x3c,	// w
	0x44, 0x28, 0x10, 0x28, 0x44,	// x
	0x0c, 0x50, 0x50, 0x50, 0x3c,	// y
	0x44, 0x64, 0x54, 0x4c, 0x44,	// z
	0x08, 0x36, 0x41, 0x41, 0x00,	// {
	0x00, 0x00, 0x77, 0x00, 0x00,	// |
	0x00, 0x41, 0x41, 0x36, 0x08,	// }
	0x02, 0x01, 0x02, 0x04, 0x02,	// ~
};

static OBJ primShapeforChar(int argCount, OBJ *args) {
	// Return a byte array with the columns (left to right) of a character from
	// the built-in the font (max 8 pixels tall). Character set is 0 to 255.

	const int fontWidth = 5;
	int ascii = -1;
	OBJ arg = args[0];
	if (isInt(arg)) {
		// argument is an integer
		ascii = evalInt(arg);
	} else if (IS_TYPE(arg, StringType) && (objWords(arg) > 0)) {
		// argument is a non-empty string; use its first (and usually only) byte
		ascii = *((uint8 *) &FIELD(arg, 0));
	}
	if ((ascii < 32) || (ascii > 126)) return zeroObj; // out of range

	// create byte array
	OBJ result = newObj(ByteArrayType, 2, falseObj); // two words, up to 8 bytes
	if (result) setByteCountAdjust(result, fontWidth); // font width

	// copy fontWidth bytes, one byte per column
	uint8 *dst = (uint8 *) &FIELD(result, 0);
	const uint8 *src = &font5x7[fontWidth * (ascii - 32)];
	memcpy(dst, src, fontWidth);
	return result;
}

static OBJ primClearGraph(int argCount, OBJ *args) {
	if (!ideConnected()) return falseObj; // do nothing if not connected to IDE
	waitAndSendMessage(clearGraphMsg, 0, 0, NULL);
	return falseObj;
}

static OBJ primFunctionExists(int argCount, OBJ *args) {
	if ((argCount < 1) || !IS_TYPE(args[0], StringType)) return fail(needsStringError);

	return (chunkIndexForFunction(obj2str(args[0])) < 0) ? falseObj : trueObj;
}

static OBJ primDUELinkPID(int argCount, OBJ *args) {
	return int2obj(*((uint32 *) 0x1FFF7004) & 0xFFFFFF);
}

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP8266)

#if defined(ARDUINO_ARCH_ESP32)
	#include <esp_sleep.h>
#else
	// Defined in ioPrims.cpp because it needs to use the ESP C++ class.
	void esp8266DeepSleep(uint64_t usecs);
#endif

static OBJ primESPSleep(int argCount, OBJ *args) {
	// Deep sleep for N seconds. When that time elapses, the ESP32 will reset/boot.
	// Note: on ESP8266, you must connect GPIO16 ("Wake" pin) to the RST to use deep sleep:
	//	https://randomnerdtutorials.com/esp8266-deep-sleep-with-arduino-ide/

	if ((argCount < 1) || !isInt(args[0])) return fail(needsIntegerError);

	uint64_t usecs = obj2int(args[0]) * 1000000;
	#if defined(ARDUINO_ARCH_ESP32)
		esp_sleep_enable_timer_wakeup(usecs);
		esp_deep_sleep_start();
	#else
		esp8266DeepSleep(usecs);
	#endif
	return falseObj; // this is never executed
}

#endif

#if defined(DUELink)

#include <stm32c0xx.h>
#include <stm32c0xx_hal_pwr.h>
#include <stm32c0xx_hal_pwr_ex.h>
#include <usbd_cdc_if.h> // for CDC_deInit()

void delay(unsigned long); // Arduino delay function

// These are the only possible wakeup pins on C071:
// WKUP1 - PA0 - Due P1
// WKUP2 - PC13 (nc) or PA4 - Due P3
// WKUP3 - PB6 (SCL) - Due P15
// WKUP4 - PA2 (UART2 TX) - off limits
// WKUP5 - PC5 (nc)
// WKUP6 - PB5 (SPI) - Due P14
// MicroBlocks currently supports only PA0, PA4 and PB5 (see below)

static OBJ primDUESleep(int argCount, OBJ *args) {
	// Some measurments:
	//	HAL_PWR_EnterSTOPMode(0, 0); // 500-750 uA (Snowy)
	//	HAL_PWR_EnterSTANDBYMode(); // 53 uA (can't recall which board; on Snowy it is < 1 uA)
	//	HAL_PWREx_EnterSHUTDOWNMode(); // < 1 uA (too low to measure)
	// Note: Boards with voltage regulators consume 1-3 mA even in shutdown mode.

	// The following allows a user to recover if they create a script like "when started, sleep"
	// It gives them ten seconds to connect the IDE to the board so they can change their code.
	if (totalMicrosecs() < (5 * 1000000)) return falseObj; // do nothing for N secs after startup

	HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH);
	HAL_PWREx_EnablePullUpPullDownConfig();
	HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_A, GPIO_PIN_0);
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF1);

	HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN2_HIGH);
	HAL_PWREx_EnablePullUpPullDownConfig();
	HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_A, GPIO_PIN_4);
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF2);

// Commented out to save space (308 bytes)
// 	if ((argCount > 0) && (args[0] == trueObj)) {
// 		// STOP mode; wakes up on alarm but uses about 1 mA
// 		CDC_deInit();
// 		HAL_SuspendTick(); // suspend tick interrupts so we don't spontaneously wake up
// 		HAL_PWR_EnterSTOPMode(0, PWR_STOPENTRY_WFI); // stop; continue from here on wakeup
// 		SystemClock_Config(); // necessary; restarts the USB clock, I think
// 		HAL_ResumeTick();
// 		CDC_init();
// 		HAL_PWREx_DisablePullUpPullDownConfig();
// 	} else {
		// default: SHUTDOWN mode; wake up on wakeup pin and uses less than 0.001 mA
		// on boards without voltage regulators (e.g. Snowy or Chrono)
		HAL_PWREx_EnterSHUTDOWNMode();
		__WFI(); // shuts down here; restarts on wakeup
// 	}

	return falseObj;
}

#include <stm32c0xx_hal_rtc.h>

static RTC_HandleTypeDef hrtc;
static bool rtc_initialized = false;

static void Rtc_Initialize() {
	if (rtc_initialized) return;

	LL_RCC_LSI_Enable(); // enable LSI clock
	while (LL_RCC_LSI_IsReady() != 1) { /* wait until ready */ }

	__HAL_RCC_SYSCFG_CLK_ENABLE();
	__HAL_RCC_PWR_CLK_ENABLE();

	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	int status = HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
	if (status != HAL_OK) {
		reportNum("HAL_RCCEx_PeriphCLKConfig error", status);
		return;
	}

	// enable peripheral clock
	__HAL_RCC_RTC_ENABLE();
	__HAL_RCC_RTCAPB_CLK_ENABLE();

	// initialize RTC
	hrtc.Instance = RTC;
	hrtc.Lock = HAL_UNLOCKED;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = 0x7F;
	hrtc.Init.SynchPrediv = 0xFF;
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
	status = HAL_RTC_Init(&hrtc);
	if (status != HAL_OK) {
		reportNum("HAL_RTC_Init error", status);
	}

	rtc_initialized = true;
}

static OBJ primDUEGetDateAndTime(int argCount, OBJ *args) {
	// Get current time and date.
	// Result is list: year month day dayOfWeek hour minute second

	if (!rtc_initialized) Rtc_Initialize();

	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	OBJ result = newObj(ListType, 8, falseObj);
	if (!result) return fail(insufficientMemoryError); // allocation failed
	FIELD(result, 0) = int2obj(7); // list size
	FIELD(result, 1) = int2obj(2000 + sDate.Year);
	FIELD(result, 2) = int2obj(sDate.Month);
	FIELD(result, 3) = int2obj(sDate.Date);
	FIELD(result, 4) = int2obj(sDate.WeekDay);
	FIELD(result, 5) = int2obj(sTime.Hours);
	FIELD(result, 6) = int2obj(sTime.Minutes);
	FIELD(result, 7) = int2obj(sTime.Seconds);
	return result;
}

static OBJ primDUESetTime(int argCount, OBJ *args) {
	// Set time: hours minutes seconds

	if (argCount < 3) return falseObj; // not enough arguments
	if (!rtc_initialized) Rtc_Initialize();

	RTC_TimeTypeDef sTime;
	sTime.Hours = obj2int(args[0]);
	sTime.Minutes = obj2int(args[1]);
	sTime.Seconds = obj2int(args[2]);
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

	return falseObj;
}

static OBJ primDUESetDate(int argCount, OBJ *args) {
	// Set date: year month day [optional: weekeday (1-7)]

	if (argCount < 3) return falseObj; // not enough arguments
	if (!rtc_initialized) Rtc_Initialize();

	int year = obj2int(args[0]);
	if (year > 2000) year -= 2000;

	RTC_DateTypeDef sDate;
	sDate.Year = year;
	sDate.Month = obj2int(args[1]);
	sDate.Date = obj2int(args[2]);
	if (argCount > 3) sDate.WeekDay = obj2int(args[3]);
	HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	return falseObj;
}

// Commented out to save space (~1000 bytes!):
// static OBJ primDUESetAlarm(int argCount, OBJ *args) {
// 	// Set the alarm: hours minutes seconds (date and weekday are ignored)
// 	// Call without arguments to disable the alarm
//
// 	if (argCount < 3) return falseObj; // not enough arguments
// 	if (!rtc_initialized) Rtc_Initialize();
//
// 	if (argCount < 3) { // disable alarm
// 		HAL_NVIC_DisableIRQ(RTC_IRQn);
// 		return falseObj;
// 	}
//
// 	RTC_AlarmTypeDef sAlarm = {0};
// 	sAlarm.Alarm = RTC_ALARM_A;
// 	sAlarm.AlarmTime.Hours = obj2int(args[0]);
// 	sAlarm.AlarmTime.Minutes = obj2int(args[1]);
// 	sAlarm.AlarmTime.Seconds = obj2int(args[2]);
// 	sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY; // ignore date and weekday
// 	sAlarm.AlarmTime.SubSeconds = 0;
//	sAlarm.AlarmSubSecondMask = 0; // ignore subseconds
//
// 	HAL_NVIC_SetPriority(RTC_IRQn, 0, 0);
// 	HAL_NVIC_EnableIRQ(RTC_IRQn);
//
// 	int status = HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN);
// 	if (status != HAL_OK) {
// 		reportNum("HAL_RTC_SetAlarm_IT error", status);
// 	}
//
// 	return falseObj;
// }
//
// void RTC_IRQHandler(void) {
// 	HAL_RTC_AlarmIRQHandler(&hrtc);
// }

#endif

// Primitives

static PrimEntry entries[] = {
	{"sqrt", primIntSqrt},
	{"sin", primIntSine},
	{"version", primVersion},
	{"bleID", primBLE_ID},
	{"hexToInt", primHexToInt},
	{"binToInt", primBinaryToInt},
	{"rescale", primRescale},
	{"connectedToIDE", primConnectedToIDE},
	{"broadcastToIDE", primBroadcastToIDEOnly},
	{"bme680GasResistance", primBMP680GasResistance},
	{"shapeforChar", primShapeforChar},
	{"clearGraph", primClearGraph},
	{"functionExists", primFunctionExists},
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP8266)
	{"espSleep", primESPSleep},
#endif
#if defined(DUELink)
	{"dueLinkPID", primDUELinkPID},
	{"dueSleep", primDUESleep},
	{"dueGetTime", primDUEGetDateAndTime},
	{"dueSetTime", primDUESetTime},
	{"dueSetDate", primDUESetDate},
//	{"dueSetAlarm", primDUESetAlarm}, // commented out to save space
#else
	{"hsvColor", primHSVColor},
	{"hue", primColorHue},
	{"saturation", primColorSaturation},
	{"brightness", primColorBrightness},
	{"atan2", primArctan},
	{"pressureToAltitude", primPressureToAltitude},
#endif
	{"jsonGet", primJSONGet},
	{"jsonCount", primJSONCount},
	{"jsonValueAt", primJSONValueAt},
	{"jsonKeyAt", primJSONKeyAt},
	{"scriptTooLarge", primScriptTooLarge},
};

void addMiscPrims() {
	addPrimitiveSet(MiscPrims, "misc", sizeof(entries) / sizeof(PrimEntry), entries);
}
