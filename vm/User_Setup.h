//#error "✅ Custom User_Setup.h is being used!"
#if defined(ARDUINO_IOT_BUS)
// https://docs.iot-bus.com/en/latest/boards/iot-bus-display.html
/*
2	DAT0 (SD Card)
4	DAT1 (SD Card)
5	SS (TFT)
12	DAT2 (SD Card)
13	DAT2 (SD Card)
14	CLK (SD Card)
15	CMD (SD)
16	SS (Touch Screen)
17	IRQ (Touch Screen)
18	SCK (TFT)
19	MISO (TFT)
23	MOSI (TFT)
27	DC (TFT)
33	Backlight (TFT) - you will not see anything if you do not turn on the backlight!
EN	RESET (TFT)
3V3	Power
GND	Ground


*/
#define ILI9341_DRIVER
//#define ILI9342_DRIVER

// SPI pin configuration

//#define TFT_WIDTH  320
//#define TFT_HEIGHT 240
#define TFT_MOSI 13 //23
#define TFT_MISO 12 //19
#define TFT_SCLK 14 //18
#define TFT_CS 15 //21 //5
#define TFT_DC 23
#define TFT_RST 25 // -1

//#define TOUCH_CS 26 //14  // 12

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE         1   

#elif defined(CYDC)
#define ILI9341_DRIVER

/*
ili9341(miso=12, mosi=13, clk=14, cs=15, dc=2, rst=-1, backlight=27, power=-1, width=320, height=240, rot=LANDSCAPE)
from machine import I2S, Pin
p=Pin(27,Pin.OUT)
p.on()
from machine import Pin, SPI, ADC, PWM, SDCard, SoftI2C
import struct
touch = SoftI2C(scl=Pin(32), sda=Pin(33), freq=400000)
touch.writeto_mem(21, 0xfe, b'\xff') #

*/
//#define ILI9342_DRIVER

// SPI pin configuration

//#define TFT_WIDTH  320
//#define TFT_HEIGHT 240
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE         1   


#elif defined(CYDR) && !defined(USB2)
#if defined(CYDROT)
    #define ILI9342_DRIVER
#else
    #define ILI9341_DRIVER
#endif

/*
ili9341(miso=12, mosi=13, clk=14, cs=15, dc=2, rst=-1, backlight=27, power=-1, width=320, height=240, rot=LANDSCAPE)
from machine import I2S, Pin
p=Pin(27,Pin.OUT)
p.on()
from machine import Pin, SPI, ADC, PWM, SDCard, SoftI2C
import struct
touch = SoftI2C(scl=Pin(32), sda=Pin(33), freq=400000)
touch.writeto_mem(21, 0xfe, b'\xff') #

*/
//#define ILI9342_DRIVER

// SPI pin configuration

//#define TFT_WIDTH  320
//#define TFT_HEIGHT 240
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#if defined(CYDROT)
  #define TFT_BL 27
#else
  #define TFT_BL 21
#endif
#define TP_CLK 25
#define TP_CS 33
#define TP_DIN 32
#define TP_DOUT 39
#define TP_IRQ 36


#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define TFT_INVERSION_OFF

#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE         1   


#elif defined(CYDR) && defined(USB2)
#define ST7789_DRIVER

/*
ili9341(miso=12, mosi=13, clk=14, cs=15, dc=2, rst=-1, backlight=27, power=-1, width=320, height=240, rot=LANDSCAPE)
from machine import I2S, Pin
p=Pin(27,Pin.OUT)
p.on()
from machine import Pin, SPI, ADC, PWM, SDCard, SoftI2C
import struct
touch = SoftI2C(scl=Pin(32), sda=Pin(33), freq=400000)
touch.writeto_mem(21, 0xfe, b'\xff') #

*/
//#define ILI9342_DRIVER

// SPI pin configuration

//#define TFT_WIDTH  320
//#define TFT_HEIGHT 240
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 21

#define TP_CLK 25
#define TP_CS 33
#define TP_DIN 32
#define TP_DOUT 39
#define TP_IRQ 36

#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_ON
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE         1   


#elif defined(LMSDISPLAY) && defined(BREAKOUT)
#define ILI9341_DRIVER
//#define ILI9342_DRIVER

// SPI pin configuration

//#define TFT_WIDTH  320
//#define TFT_HEIGHT 240

/*
2	DAT0 (SD Card)
4	DAT1 (SD Card)
5	SS (TFT)
12	DAT2 (SD Card)
13	DAT2 (SD Card)
14	CLK (SD Card)
15	CMD (SD)
16	SS (Touch Screen)
17	IRQ (Touch Screen)
18	SCK (TFT)
19	MISO (TFT)
23	MOSI (TFT)
27	DC (TFT)
33	Backlight (TFT) - you will not see anything if you do not turn on the backlight!
EN	RESET (TFT)
3V3	Power
GND	Ground


LMSDISPLAY
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 27
#define TFT_RST -1

*/




#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_CS 5
#define TFT_DC 27
#define TFT_RST -1

#define TOUCH_CS 16  // For XPT2046

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE         1   

#elif defined(LMSDISPLAY) && !defined(BREAKOUT)
#define ILI9341_DRIVER
//#define ILI9342_DRIVER

// SPI pin configuration

//#define TFT_WIDTH  320
//#define TFT_HEIGHT 240
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 27
#define TFT_RST 32

#define TOUCH_CS 26  // For XPT2046

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT

#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE         1   
#elif defined(ARDUINO_M5STACK_Core2)

//#define ILI9341_DRIVER
#define ILI9341_DRIVER

// SPI pin configuration

//#define TFT_WIDTH  320
//#define TFT_HEIGHT 240
#define TFT_MOSI 23
#define TFT_MISO 38
#define TFT_SCLK 18
#define TFT_CS 5
#define TFT_DC 15
#define TFT_RST 32

//#define TOUCH_CS 26  // For XPT2046

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT
#define TFT_ROTATION 2
#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2
// swap R and B
//#define TFT_RGB_ORDER TFT_BGR 

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE         1  
#elif defined(LMS7789)
#define ST7789_DRIVER

#define TFT_MOSI 26
#define TFT_MISO -1
#define TFT_SCLK 15
#define TFT_CS 14

#define TFT_DC 27
#define TFT_RST 13

#define TFT_BL 12
#define TFT_BACKLIGHT_ON HIGH  

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT
//#define TFT_ROTATION 2
#define TFT_SPI_PORT HSPI 
// Optional: set SPI clock speed
#define CONFIG_TFT_HSPI_PORT
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

#define TFT_WIDTH 240
#define TFT_HEIGHT 280

// Optional: enable touch or fonts
//#define SUPPORT_TOUCH
//#define LOAD_GLCD
//#define LOAD_FONT2
// swap R and B
#define TFT_RGB_ORDER TFT_RGB

#define SUPPORT_TRANSACTIONS
#define USE_HSPI_PORT         // Use HSPI (VSPI also works, but this matches the custom pins above)
#define ENABLE_8_BIT_PALETTES
#define TFT_DMA_BUFFER_SIZE  132768
#define TFT_DMA_MODE  

#endif