// ST7789 driver for 8-bit parallel (Intel 8080) panels, such as the one on the LilyGO T-Display S3.
// It mirrors the queueing model of the ILI9341 SPI driver: rg_display takes a buffer, fills it, sends it,
// and the buffer comes back to the free queue once the DMA transfer is done.

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_io.h>
#include <esp_idf_version.h>

static esp_lcd_i80_bus_handle_t i80_bus;
static esp_lcd_panel_io_handle_t i80_io;
static QueueHandle_t i80_buffers;  // Free buffers
static QueueHandle_t i80_inflight; // Buffers queued for transfer, in submission order
static int i80_ram_cmd = 0x2C;     // RAMWR for the first chunk of a window, RAMWRC for the following ones

#define I80_TRANSACTION_COUNT (10)
#define I80_BUFFER_COUNT      (5)
#define I80_BUFFER_LENGTH     (LCD_BUFFER_LENGTH * 2) // In bytes

#ifndef RG_SCREEN_OFFSET_X
#define RG_SCREEN_OFFSET_X 0
#endif
#ifndef RG_SCREEN_OFFSET_Y
#define RG_SCREEN_OFFSET_Y 0
#endif

// Same name as the SPI driver so that RG_SCREEN_INIT() sequences work unchanged.
// esp_lcd's i80 tx_param waits for queued color transfers to finish, so ordering is preserved.
#define ILI9341_CMD(cmd, data...)                                                  \
    {                                                                              \
        const uint8_t x[] = {data};                                                \
        esp_lcd_panel_io_tx_param(i80_io, cmd, sizeof(x) ? x : NULL, sizeof(x));   \
    }

static inline uint16_t *i80_take_buffer(void)
{
    uint16_t *buffer;
    if (xQueueReceive(i80_buffers, &buffer, pdMS_TO_TICKS(2500)) != pdTRUE)
        RG_PANIC("display");
    return buffer;
}

static inline void i80_give_buffer(uint16_t *buffer)
{
    xQueueSend(i80_buffers, &buffer, portMAX_DELAY);
}

// Transfers complete in the order they were queued, so the finished buffer is the oldest in-flight one.
IRAM_ATTR
static bool i80_color_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    uint16_t *buffer;
    if (xQueueReceiveFromISR(i80_inflight, &buffer, &woken) == pdTRUE)
        xQueueSendFromISR(i80_buffers, &buffer, &woken);
    return woken == pdTRUE;
}

static void i80_init(void)
{
    i80_buffers = xQueueCreate(I80_BUFFER_COUNT, sizeof(uint16_t *));
    i80_inflight = xQueueCreate(I80_BUFFER_COUNT, sizeof(uint16_t *));

    while (uxQueueSpacesAvailable(i80_buffers))
    {
        void *buffer = rg_alloc(I80_BUFFER_LENGTH, MEM_DMA);
        xQueueSend(i80_buffers, &buffer, portMAX_DELAY);
    }

    const esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = RG_GPIO_LCD_DC,
        .wr_gpio_num = RG_GPIO_LCD_WR,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .clk_src = LCD_CLK_SRC_DEFAULT,
#else
        .clk_src = LCD_CLK_SRC_PLL160M,
#endif
        .data_gpio_nums = RG_GPIO_LCD_DATA,
        .bus_width = 8,
        .max_transfer_bytes = I80_BUFFER_LENGTH,
    };
    esp_err_t ret = esp_lcd_new_i80_bus(&bus_config, &i80_bus);
    RG_ASSERT(ret == ESP_OK, "esp_lcd_new_i80_bus failed.");

    const esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = RG_GPIO_LCD_CS,
        .pclk_hz = RG_SCREEN_SPEED,
        .trans_queue_depth = I80_TRANSACTION_COUNT,
        .on_color_trans_done = &i80_color_trans_done,
        .user_ctx = NULL,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
    };
    ret = esp_lcd_new_panel_io_i80(i80_bus, &io_config, &i80_io);
    RG_ASSERT(ret == ESP_OK, "esp_lcd_new_panel_io_i80 failed.");
}

static void i80_deinit(void)
{
    if (i80_io)
        esp_lcd_panel_io_del(i80_io);
    if (i80_bus)
        esp_lcd_del_i80_bus(i80_bus);
    i80_io = NULL;
    i80_bus = NULL;
}

static void lcd_set_backlight(float percent)
{
    float level = RG_MIN(RG_MAX(percent / 100.f, 0), 1.f);
    int error_code = 0;

#if defined(RG_GPIO_LCD_BCKL)
    // Channel/timer 1 because the buzzer audio driver uses channel/timer 0
    error_code = ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0x1FFF * level, 50, 0);
#endif

    if (error_code)
        RG_LOGE("failed setting backlight to %d%% (0x%02X)\n", (int)(100 * level), error_code);
    else
        RG_LOGI("backlight set to %d%%\n", (int)(100 * level));
}

static void lcd_set_window(int left, int top, int width, int height)
{
    int right = left + width - 1;
    int bottom = top + height - 1;

    if (left < 0 || top < 0 || right >= display.screen.real_width || bottom >= display.screen.real_height)
        RG_LOGW("Bad lcd window (x0=%d, y0=%d, x1=%d, y1=%d)\n", left, top, right, bottom);

    left += RG_SCREEN_OFFSET_X;
    right += RG_SCREEN_OFFSET_X;
    top += RG_SCREEN_OFFSET_Y;
    bottom += RG_SCREEN_OFFSET_Y;

    ILI9341_CMD(0x2A, left >> 8, left & 0xff, right >> 8, right & 0xff); // Horiz
    ILI9341_CMD(0x2B, top >> 8, top & 0xff, bottom >> 8, bottom & 0xff); // Vert
    i80_ram_cmd = 0x2C; // Memory write is sent with the first chunk of pixels
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    return i80_take_buffer();
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    if (length == 0)
    {
        i80_give_buffer(buffer);
        return;
    }

    // Must be queued before the transfer starts, the completion ISR pops it
    xQueueSend(i80_inflight, &buffer, portMAX_DELAY);
    if (esp_lcd_panel_io_tx_color(i80_io, i80_ram_cmd, buffer, length * sizeof(*buffer)) != ESP_OK)
        RG_PANIC("display");
    i80_ram_cmd = 0x3C; // Memory write continue
}

static void lcd_sync(void)
{
    // Unused, transfers are already ordered by esp_lcd
}

static void lcd_init(void)
{
#ifdef RG_GPIO_LCD_POWER
    // On the T-Display S3 this switches the LCD power rail, required when running from battery
    gpio_set_direction(RG_GPIO_LCD_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(RG_GPIO_LCD_POWER, 1);
#endif

#ifdef RG_GPIO_LCD_RD
    // We never read from the panel, but RD must be held high
    gpio_set_direction(RG_GPIO_LCD_RD, GPIO_MODE_OUTPUT);
    gpio_set_level(RG_GPIO_LCD_RD, 1);
#endif

#ifdef RG_GPIO_LCD_BCKL
    // Initialize backlight at 0% to avoid the lcd reset flash
    ledc_timer_config(&(ledc_timer_config_t){
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 5000,
    });
    ledc_channel_config(&(ledc_channel_config_t){
        .gpio_num = RG_GPIO_LCD_BCKL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
    #ifdef RG_GPIO_LCD_BCKL_INVERT
        .flags.output_invert = 1,
    #endif
    });
    ledc_fade_func_install(0);
#endif

    i80_init();

#if defined(RG_GPIO_LCD_RST)
    gpio_set_direction(RG_GPIO_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(RG_GPIO_LCD_RST, 0);
    rg_usleep(100 * 1000);
    gpio_set_level(RG_GPIO_LCD_RST, 1);
    rg_usleep(10 * 1000);
#endif

    ILI9341_CMD(0x01);       // Reset
    rg_usleep(120 * 1000);   // ST7789 needs 120ms after a software reset
    ILI9341_CMD(0x3A, 0X55); // COLMOD (Pixel Format Set RGB565 65k)
#ifdef RG_SCREEN_INIT
    RG_SCREEN_INIT();
#else
    #warning "LCD init sequence is not defined for this device!"
#endif
    ILI9341_CMD(0x11);     // Exit Sleep
    rg_usleep(120 * 1000); // Wait 120ms after sleep out
    ILI9341_CMD(0x29);     // Display on
}

static void lcd_deinit(void)
{
#ifdef RG_SCREEN_DEINIT
    RG_SCREEN_DEINIT();
#endif
    i80_deinit();
}

const rg_display_driver_t rg_display_driver_st7789_i80 = {
    .name = "st7789_i80",
};
