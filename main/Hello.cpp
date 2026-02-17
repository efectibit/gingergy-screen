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
#define LCD_PIXEL_CLOCK_HZ     (12 * 1000 * 1000)

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
	lv_obj_set_style_bg_color(scr, lv_color_hex(0xEEEEEE), 0); // Fondo gris claro

	// --- 1. FILA SUPERIOR: ESTADO DE TERMINALES ---
	lv_obj_t *top_cont = lv_obj_create(scr);
	lv_obj_set_size(top_cont, 800, 100);
	lv_obj_align(top_cont, LV_ALIGN_TOP_MID, 0, 10);
	lv_obj_set_style_bg_opa(top_cont, 0, 0);
	lv_obj_set_style_border_width(top_cont, 0, 0);
	lv_obj_set_flex_flow(top_cont, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(top_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

	const char *ids[] = {"01", "02", "03", "04", "05", "06"};
	lv_color_t colors[] = {lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_YELLOW),
						   lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE),
						   lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_BLUE)};

	for (int i = 0; i < 6; i++) {
		lv_obj_t *item = lv_obj_create(top_cont);
		lv_obj_set_size(item, 80, 80);
		lv_obj_set_style_bg_opa(item, 0, 0);
		lv_obj_set_style_border_width(item, 0, 0);

		lv_obj_t *icon = lv_label_create(item);
		lv_label_set_text(icon, LV_SYMBOL_CHARGE);
		lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
		lv_obj_set_style_text_color(icon, colors[i], 0);
		lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

		lv_obj_t *lbl = lv_label_create(item);
		lv_label_set_text(lbl, ids[i]);
		lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
	}

	// --- 2. TÍTULO CENTRAL ---
	lv_obj_t *term_title = lv_label_create(scr);
	lv_label_set_text(term_title, "TERMINAL 05");
	lv_obj_set_style_text_font(term_title, &lv_font_montserrat_18, 0);
	lv_obj_align(term_title, LV_ALIGN_TOP_MID, 0, 120);

	// --- 3. ARCO CENTRAL ---
	lv_obj_t *arc = lv_arc_create(scr);
	lv_obj_set_size(arc, 350, 350);
	lv_arc_set_rotation(arc, 180);
	lv_arc_set_bg_angles(arc, 0, 180);
	lv_arc_set_value(arc, 50);
	lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
	lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(arc, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_arc_color(arc, lv_color_black(), LV_PART_INDICATOR);
	lv_obj_remove_style(arc, NULL, LV_PART_KNOB); // Quita el círculo del indicador
	lv_obj_align(arc, LV_ALIGN_CENTER, 0, 100);

	// --- 4. TIEMPO Y CONTROLES ---
	lv_obj_t *time_main = lv_label_create(scr);
	lv_label_set_text(time_main, "1:15");
	lv_obj_set_style_text_font(time_main, &lv_font_montserrat_48, 0);
	lv_obj_align(time_main, LV_ALIGN_CENTER, 0, 30);

	lv_obj_t *time_sub = lv_label_create(scr);
	lv_label_set_text(time_sub, "15 minutos");
	lv_obj_align(time_sub, LV_ALIGN_CENTER, 0, 80);

	// Botones "-" y "+"
	lv_obj_t *btn_min = lv_button_create(scr);
	lv_obj_set_size(btn_min, 60, 60);
	lv_obj_align(btn_min, LV_ALIGN_CENTER, -100, 80);
	lv_obj_set_style_bg_color(btn_min, lv_color_hex(0xBDC3C7), 0);
	lv_obj_t *lbl_min = lv_label_create(btn_min);
	lv_label_set_text(lbl_min, "-");
	lv_obj_center(lbl_min);

	lv_obj_t *btn_plus = lv_button_create(scr);
	lv_obj_set_size(btn_plus, 60, 60);
	lv_obj_align(btn_plus, LV_ALIGN_CENTER, 100, 80);
	lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0xBDC3C7), 0);
	lv_obj_t *lbl_plus = lv_label_create(btn_plus);
	lv_label_set_text(lbl_plus, "+");
	lv_obj_center(lbl_plus);

	// --- 5. BOTÓN USAR ---
	lv_obj_t *btn_use = lv_button_create(scr);
	lv_obj_set_size(btn_use, 180, 60);
	lv_obj_align(btn_use, LV_ALIGN_BOTTOM_MID, 0, -50);
	lv_obj_set_style_bg_color(btn_use, lv_color_hex(0xA3D9A5), 0); // Verde suave
	lv_obj_set_style_border_width(btn_use, 2, 0);
	lv_obj_set_style_border_color(btn_use, lv_color_black(), 0);

	lv_obj_t *lbl_use = lv_label_create(btn_use);
	lv_label_set_text(lbl_use, "USAR");
	lv_obj_set_style_text_color(lbl_use, lv_color_black(), 0);
	lv_obj_set_style_text_font(lbl_use, &lv_font_montserrat_22, 0);
	lv_obj_center(lbl_use);

	// Flechas laterales (Símbolos)
	lv_obj_t *arrow_l = lv_label_create(scr);
	lv_label_set_text(arrow_l, LV_SYMBOL_LEFT);
	lv_obj_set_style_text_font(arrow_l, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(arrow_l, lv_palette_main(LV_PALETTE_GREEN), 0);
	lv_obj_align(arrow_l, LV_ALIGN_LEFT_MID, 40, 50);

	lv_obj_t *arrow_r = lv_label_create(scr);
	lv_label_set_text(arrow_r, LV_SYMBOL_RIGHT);
	lv_obj_set_style_text_font(arrow_r, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(arrow_r, lv_palette_main(LV_PALETTE_GREEN), 0);
	lv_obj_align(arrow_r, LV_ALIGN_RIGHT_MID, -40, 50);
}
