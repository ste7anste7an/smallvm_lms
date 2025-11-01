#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lfs.h"   // Include LittleFS header

#define CONFIG_FILE "/config.txt"

typedef struct {
    char controller[16];
    int mosi, miso, sck, cs;
    int dc, rst;
    int orientation;
    int color;
    int backlight;
    int width, height;
} TFTConfig;

typedef struct {
    char interface[8];    // "i2c" or "spi"
    char spi_mode[10];    // "shared" or "separate"
    int shared_cs;
    int miso, mosi, sck, cs;
    int orientation;
    int sda, scl;
} TouchConfig;

typedef struct {
    int advanced_serial_commands;
} OtherConfig;

typedef struct {
    TFTConfig tft;
    TouchConfig touch;
    OtherConfig other;
} Config;
