#define ST7796_DRIVER     // Driver khusus untuk varian 3,5 inch

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1 
#define TFT_BL   27       // Backlight pada seri 3,5" seringkali di pin 27
#define TFT_BACKLIGHT_ON HIGH

#define TOUCH_CS 33

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  2000000
#define SPI_TOUCH_FREQUENCY  2500000