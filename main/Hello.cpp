#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "Sunton_S3";

// Parámetros Sunton 7"
#define LCD_H_RES              800
#define LCD_V_RES              480
#define LCD_PIXEL_CLOCK_HZ     (16 * 1000 * 1000)

#define PIN_NUM_BK_LIGHT       2
#define PIN_NUM_LCD_RST        38
#define PIN_NUM_HSYNC          39
#define PIN_NUM_VSYNC          40
#define PIN_NUM_DE             41
#define PIN_NUM_PCLK           42

// Semáforo Global
SemaphoreHandle_t xGuiSemaphore = NULL;

void create_demo_ui(lv_display_t *disp);

// Callback de fin de transferencia (ISR)
static bool IRAM_ATTR notify_lvgl_flush_ready(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_ctx) {
	lv_display_t *disp = (lv_display_t *)user_ctx;
	lv_display_flush_ready(disp);
	return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
	esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
	esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

static void tick_inc_cb(void *arg) {
	lv_tick_inc(5);
}

static void guiTask(void *pvParameter) {
	esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)pvParameter;

	// Crear el semáforo Mutex
	xGuiSemaphore = xSemaphoreCreateMutex();

	lv_init();

	// Configurar el Timer del Tick de LVGL (como en tu ejemplo)
	const esp_timer_create_args_t periodic_timer_args = {
		.callback = &tick_inc_cb,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "lvgl_tick",
		.skip_unhandled_events = false,
	};
	esp_timer_handle_t periodic_timer;
	esp_timer_create(&periodic_timer_args, &periodic_timer);
	esp_timer_start_periodic(periodic_timer, 5 * 1000); // 5ms

	// Display LVGL
	lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
	lv_display_set_user_data(display, (void*)panel_handle);
	lv_display_set_flush_cb(display, lvgl_flush_cb);

	// Buffer en SRAM interna
	size_t draw_buf_sz = LCD_H_RES * 40 * sizeof(lv_color_t);
	void *buf1 = heap_caps_malloc(draw_buf_sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	lv_display_set_buffers(display, buf1, NULL, draw_buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);

	// Registrar callback de sincronización
	esp_lcd_rgb_panel_event_callbacks_t cbs = {
		.on_color_trans_done = notify_lvgl_flush_ready,
		.on_vsync = NULL,
		.on_bounce_empty = NULL,
	};
	esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, display);

	ESP_LOGI(TAG, "LVGL Iniciado. Creando UI...");

	// Tomar semáforo para crear la UI inicial
	if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
		create_demo_ui(display);
		xSemaphoreGive(xGuiSemaphore);
	}

	while (1) {
		// Delay de 10ms para no saturar la CPU
		vTaskDelay(pdMS_TO_TICKS(10));

		// Ejecutar handler de LVGL protegiendo con el semáforo
		if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
			lv_timer_handler();
			xSemaphoreGive(xGuiSemaphore);
		}
	}
}

extern "C" void app_main() {
	// 1. Reset Hardware
	gpio_set_direction((gpio_num_t)PIN_NUM_LCD_RST, GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t)PIN_NUM_LCD_RST, 0);
	vTaskDelay(pdMS_TO_TICKS(50));
	gpio_set_level((gpio_num_t)PIN_NUM_LCD_RST, 1);
	vTaskDelay(pdMS_TO_TICKS(120));

	// 2. Backlight
	gpio_set_direction((gpio_num_t)PIN_NUM_BK_LIGHT, GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t)PIN_NUM_BK_LIGHT, 1);

	// 3. Configuración del Panel RGB
	esp_lcd_panel_handle_t panel_handle = NULL;
	esp_lcd_rgb_panel_config_t panel_config = {
		.clk_src = LCD_CLK_SRC_DEFAULT,
		.timings = {
			.pclk_hz = LCD_PIXEL_CLOCK_HZ,
			.h_res = LCD_H_RES,
			.v_res = LCD_V_RES,
			.hsync_pulse_width = 30,
			.hsync_back_porch = 16,
			.hsync_front_porch = 210,
			.vsync_pulse_width = 13,
			.vsync_back_porch = 10,
			.vsync_front_porch = 22,
			.flags = {
				.hsync_idle_low = 0,
				.vsync_idle_low = 0,
				.de_idle_high = 0,
				.pclk_active_neg = 1,
				.pclk_idle_high = 0,
			},
		},
		.data_width = 16,
		.bits_per_pixel = 16,
		.num_fbs = 1,
		.hsync_gpio_num = PIN_NUM_HSYNC,
		.vsync_gpio_num = PIN_NUM_VSYNC,
		.de_gpio_num = PIN_NUM_DE,
		.pclk_gpio_num = PIN_NUM_PCLK,
		.data_gpio_nums = {
			15, 7, 6, 5, 4,       // Blue
			9, 46, 3, 8, 16, 1,   // Green
			14, 21, 47, 48, 45    // Red
		},
		.flags = {
			.disp_active_low = 0,
			.refresh_on_demand = 0,
			.fb_in_psram = 1,
			.double_fb = 0,
			.no_fb = 0,
			.bb_invalidate_cache = 0,
		},
	};

	ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
	ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
	ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

	// 4. Crear tarea de GUI en el Core 1
	xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, (void*)panel_handle, 5, NULL, 1);
}

void create_demo_ui(lv_display_t *disp) {
	lv_obj_t *scr = lv_display_get_screen_active(disp);
	lv_obj_set_style_bg_color(scr, lv_palette_main(LV_PALETTE_BLUE), 0);

	lv_obj_t *label = lv_label_create(scr);
	lv_label_set_text(label, "S3 Nativo: OK");
	lv_obj_set_style_text_color(label, lv_color_black(), 0);
	lv_obj_center(label);
}
