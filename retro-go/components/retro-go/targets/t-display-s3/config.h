/* Configuration for the LilyGO T-Display S3 (non-touch), ESP32-S3R8 with 16MB flash and 8MB PSRAM.
 *
 * Hardware info: https://github.com/Xinyuan-LilyGO/T-Display-S3
 *
 * The board has no SD card slot: a microSD adapter wired to the pins below is used when a card is
 * present, otherwise storage falls back to a FAT partition in the internal flash.
 *
 * External wiring: A, B, Start and Select go between their GPIO and GND (internal pull-ups are used).
 * The D-pad needs both of its axes on a single pin each, which frees the pins used by the SD adapter
 * and the DAC: Each axis pin has a 10K pull-up to 3V3, Up and Left short it to GND, Down and Right
 * pull it down through another 10K.
 * Audio goes through a PCM5102A I2S DAC module.
 */

// Target definition
#define RG_TARGET_NAME             "T-DISPLAY-S3"

// Storage
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDSPI_HOST       SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT
#define RG_STORAGE_FLASH_PARTITION  "vfs"

// Audio
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        1   // 0 = Disable, 1 = Enable

// Video
#define RG_SCREEN_DRIVER            2   // 2 = ST7789 8-bit parallel (i80)
#define RG_SCREEN_SPEED             (16 * 1000 * 1000) // LilyGO's examples use 16MHz, too high causes artifacts
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            170
#define RG_SCREEN_OFFSET_X          0   // The 170px panel sits in the middle of the ST7789's 240px RAM
#define RG_SCREEN_OFFSET_Y          35
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}  // Left, Top, Right, Bottom
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}  // Left, Top, Right, Bottom
// Init sequence from LilyGO's factory example
#define RG_SCREEN_INIT()                                                                                         \
    ILI9341_CMD(0x36, 0xA0);                 /* MADCTL (MY|MV) landscape, use 0x60 to flip it 180 degrees */     \
    ILI9341_CMD(0xB2, 0x0B, 0x0B, 0x00, 0x33, 0x33); /* Porch setting */                                        \
    ILI9341_CMD(0xB7, 0x75);                 /* Gate control */                                                  \
    ILI9341_CMD(0xBB, 0x28);                 /* VCOM setting */                                                  \
    ILI9341_CMD(0xC0, 0x2C);                 /* LCM control */                                                   \
    ILI9341_CMD(0xC2, 0x01);                 /* VDV and VRH command enable */                                    \
    ILI9341_CMD(0xC3, 0x1F);                 /* VRH set */                                                       \
    ILI9341_CMD(0xC6, 0x13);                 /* Frame rate control */                                            \
    ILI9341_CMD(0xD0, 0xA7);                                                                                     \
    ILI9341_CMD(0xD0, 0xA4, 0xA1);           /* Power control */                                                 \
    ILI9341_CMD(0xD6, 0xA1);                                                                                     \
    ILI9341_CMD(0xE0, 0xF0, 0x05, 0x0A, 0x06, 0x06, 0x03, 0x2B, 0x32, 0x43, 0x36, 0x11, 0x10, 0x2B, 0x32);       \
    ILI9341_CMD(0xE1, 0xF0, 0x08, 0x0C, 0x0B, 0x09, 0x24, 0x2B, 0x22, 0x43, 0x38, 0x15, 0x16, 0x2F, 0x37);       \
    ILI9341_CMD(0x21);                       /* Display inversion on, this panel needs it */


// Input
// Refer to rg_input.h to see all available RG_KEY_* and RG_GAMEPAD_*_MAP types
#define RG_RECOVERY_BTN RG_KEY_MENU // Keep this button pressed to open the recovery menu

// Pin at 3V3 when idle, 0V for Up/Left and around 1.65V for Down/Right
#define RG_GAMEPAD_ADC_FILTER_WINDOW 300
#define RG_GAMEPAD_ADC_PULLUPS {GPIO_NUM_1, GPIO_NUM_2} // Keeps the D-pad quiet until it is wired
#define RG_GAMEPAD_ADC_MAP {\
    {RG_KEY_UP,    ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_11, 0, 700},\
    {RG_KEY_DOWN,  ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_11, 1300, 3000},\
    {RG_KEY_LEFT,  ADC_UNIT_1, ADC_CHANNEL_1, ADC_ATTEN_DB_11, 0, 700},\
    {RG_KEY_RIGHT, ADC_UNIT_1, ADC_CHANNEL_1, ADC_ATTEN_DB_11, 1300, 3000},\
}

// MENU and OPTION are the two buttons on the board itself
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_A,      .num = GPIO_NUM_12, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_13, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_16, .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_21, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_0,  .pullup = 1, .level = 0},\
    {RG_KEY_OPTION, .num = GPIO_NUM_14, .pullup = 1, .level = 0},\
}

// Battery
// GPIO4 = ADC1_CHANNEL_3, behind a 1:2 voltage divider
#define RG_BATTERY_DRIVER           1
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_3
#define RG_BATTERY_CALC_PERCENT(raw) (((raw) * 2.f - 3500.f) / (4200.f - 3500.f) * 100.f)
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 2.f * 0.001f)

// SPI SD Card
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_18
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_43
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_44
#define RG_GPIO_SDSPI_CS            GPIO_NUM_3

// External I2S DAC
#define RG_GPIO_SND_I2S_BCK         17
#define RG_GPIO_SND_I2S_WS          10
#define RG_GPIO_SND_I2S_DATA        11

// Parallel Display
#define RG_GPIO_LCD_POWER           GPIO_NUM_15
#define RG_GPIO_LCD_BCKL            GPIO_NUM_38
#define RG_GPIO_LCD_RST             GPIO_NUM_5
#define RG_GPIO_LCD_CS              GPIO_NUM_6
#define RG_GPIO_LCD_DC              GPIO_NUM_7
#define RG_GPIO_LCD_WR              GPIO_NUM_8
#define RG_GPIO_LCD_RD              GPIO_NUM_9
#define RG_GPIO_LCD_DATA            {39, 40, 41, 42, 45, 46, 47, 48}
