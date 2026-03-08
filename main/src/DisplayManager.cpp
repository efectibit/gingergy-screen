#include "DisplayManager.hpp"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

static const char* TAG_DM = "DisplayManager";

// ─── Parámetros hardware Sunton 7" 800×480 ───────────────────────────────────
static constexpr int LCD_H_RES           = 800;
static constexpr int LCD_V_RES           = 480;
static constexpr int LCD_PIXEL_CLOCK_HZ  = 12 * 1000 * 1000;

static constexpr gpio_num_t PIN_BK_LIGHT = (gpio_num_t)2;
static constexpr gpio_num_t PIN_LCD_RST  = (gpio_num_t)38;
static constexpr gpio_num_t PIN_HSYNC    = (gpio_num_t)39;
static constexpr gpio_num_t PIN_VSYNC    = (gpio_num_t)40;
static constexpr gpio_num_t PIN_DE       = (gpio_num_t)41;
static constexpr gpio_num_t PIN_PCLK     = (gpio_num_t)42;

// ─── Touch GT911 ─────────────────────────────────────────────────────────────
static constexpr int TOUCH_I2C_NUM       = 0;
static constexpr int TOUCH_I2C_SDA       = 19;
static constexpr int TOUCH_I2C_SCL       = 20;

// ─── Tamaño del buffer de renderizado (40 líneas) ────────────────────────────
static constexpr size_t DRAW_BUF_LINES = 40;

// =============================================================================
// Constructor
// =============================================================================
DisplayManager::DisplayManager()
	: m_panelHandle(nullptr)
	, m_touchHandle(nullptr)
	, m_lvglDisplay(nullptr)
	, m_lvglIndev(nullptr)
	, m_guiSemaphore(nullptr)
	, m_tickTimer(nullptr)
{}

// =============================================================================
// init() — Inicializa el panel RGB físico
// =============================================================================
void DisplayManager::init() {
	// 1. Reset de hardware
	gpio_set_direction(PIN_LCD_RST, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_LCD_RST, 0);
	vTaskDelay(pdMS_TO_TICKS(50));
	gpio_set_level(PIN_LCD_RST, 1);
	vTaskDelay(pdMS_TO_TICKS(120));

	// 2. Encender backlight
	gpio_set_direction(PIN_BK_LIGHT, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_BK_LIGHT, 1);

	// 3. Configurar panel RGB
	esp_lcd_rgb_panel_config_t panel_config = {
		.clk_src  = LCD_CLK_SRC_DEFAULT,
		.timings  = {
			.pclk_hz          = (uint32_t)LCD_PIXEL_CLOCK_HZ,
			.h_res            = LCD_H_RES,
			.v_res            = LCD_V_RES,
			.hsync_pulse_width = 30,
			.hsync_back_porch  = 16,
			.hsync_front_porch = 210,
			.vsync_pulse_width = 13,
			.vsync_back_porch  = 10,
			.vsync_front_porch = 22,
			.flags = {
				.hsync_idle_low  = 0,
				.vsync_idle_low  = 0,
				.de_idle_high    = 0,
				.pclk_active_neg = 1,
				.pclk_idle_high  = 0,
			},
		},
		.data_width    = 16,
		.bits_per_pixel = 16,
		.num_fbs       = 1,
		.hsync_gpio_num = PIN_HSYNC,
		.vsync_gpio_num = PIN_VSYNC,
		.de_gpio_num    = PIN_DE,
		.pclk_gpio_num  = PIN_PCLK,
		.data_gpio_nums = {
			15, 7, 6, 5, 4,        // Blue [4:0]
			9, 46, 3, 8, 16, 1,    // Green [5:0]
			14, 21, 47, 48, 45     // Red [4:0]
		},
		.flags = {
			.disp_active_low    = 0,
			.refresh_on_demand  = 0,
			.fb_in_psram        = 1,
			.double_fb          = 0,
			.no_fb              = 0,
			.bb_invalidate_cache = 0,
		},
	};

	ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &m_panelHandle));
	ESP_ERROR_CHECK(esp_lcd_panel_reset(m_panelHandle));
	ESP_ERROR_CHECK(esp_lcd_panel_init(m_panelHandle));

	ESP_LOGI(TAG_DM, "Panel RGB inicializado (%dx%d)", LCD_H_RES, LCD_V_RES);

	// 4. Inicializar I2C para Touch (Nuevo driver i2c_master.h de ESP-IDF v5+)
	i2c_master_bus_config_t i2c_mst_config = {
		.i2c_port = TOUCH_I2C_NUM,
		.sda_io_num = (gpio_num_t)TOUCH_I2C_SDA,
		.scl_io_num = (gpio_num_t)TOUCH_I2C_SCL,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.intr_priority = 0,
		.trans_queue_depth = 0,
		.flags = {
			.enable_internal_pullup = true,
		},
	};
	i2c_master_bus_handle_t i2c_bus = nullptr;
	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus));

	// 5. Inicializar Controlador Touch GT911
	esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
	esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
	tp_io_config.scl_speed_hz = 400 * 1000; // <- Req. por nuevo driver i2c_master

	// En el nuevo driver, esp_lcd_new_panel_io_i2c espera el i2c_master_bus_handle_t
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

	esp_lcd_touch_config_t tp_cfg = {
		.x_max = LCD_H_RES,
		.y_max = LCD_V_RES,
		.rst_gpio_num = (gpio_num_t)38, // Comparte reset físico con display
		.int_gpio_num = (gpio_num_t)-1,
		.levels = {
			.reset = 0,
			.interrupt = 0,
		},
		.flags = {
			.swap_xy = 0,
			.mirror_x = 0,
			.mirror_y = 0,
		},
		.process_coordinates = nullptr,
		.interrupt_callback = nullptr,
	};
	ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &m_touchHandle));

	ESP_LOGI(TAG_DM, "Touch GT911 inicializado vía I2C");
}

// =============================================================================
// startGuiTask() — Arranca la tarea FreeRTOS de LVGL en el Core 1
// =============================================================================
void DisplayManager::startGuiTask(UiReadyCallback uiReadyCb) {
	m_uiReadyCb = uiReadyCb;
	// Pasar `this` como parámetro para que guiTaskFn pueda acceder a la instancia
	xTaskCreatePinnedToCore(guiTaskFn, "lvgl_gui", 4096 * 3,
							static_cast<void*>(this), 5, nullptr, 1);
}

// =============================================================================
// lock() / unlock() — Protección del acceso a LVGL
// =============================================================================
void DisplayManager::lock() {
	xSemaphoreTakeRecursive(m_guiSemaphore, portMAX_DELAY);
}

void DisplayManager::unlock() {
	xSemaphoreGiveRecursive(m_guiSemaphore);
}

lv_display_t* DisplayManager::getDisplay() {
	return m_lvglDisplay;
}

// =============================================================================
// Callbacks estáticos
// =============================================================================

/**
 * @brief ISR: notifica a LVGL que el frame fue transferido al panel.
 *        user_ctx es el puntero al lv_display_t*.
 */
bool IRAM_ATTR DisplayManager::onTransferDone(
	esp_lcd_panel_handle_t /*panel*/,
	const esp_lcd_rgb_panel_event_data_t* /*data*/,
	void* userCtx)
{
	lv_display_t* disp = static_cast<lv_display_t*>(userCtx);
	lv_display_flush_ready(disp);
	return false;
}

/**
 * @brief LVGL solicita enviar un área de píxeles al panel.
 *        user_data del display es el esp_lcd_panel_handle_t.
 */
void DisplayManager::lvglFlushCb(lv_display_t* disp,
								  const lv_area_t* area,
								  uint8_t* pxMap)
{
	auto panelHandle = static_cast<esp_lcd_panel_handle_t>(
		lv_display_get_user_data(disp));
	esp_lcd_panel_draw_bitmap(panelHandle,
							  area->x1, area->y1,
							  area->x2 + 1, area->y2 + 1,
							  pxMap);
}

/**
 * @brief Timer de 5 ms que avanza el tick interno de LVGL.
 */
void DisplayManager::tickIncCb(void* /*arg*/) {
	lv_tick_inc(5);
}

/**
 * @brief Tarea FreeRTOS del loop de LVGL.
 *        param es DisplayManager*.
 */
void DisplayManager::guiTaskFn(void* param) {
	auto* self = static_cast<DisplayManager*>(param);

	// --- Mutex ---
	self->m_guiSemaphore = xSemaphoreCreateRecursiveMutex();

	// --- Inicializar LVGL ---
	lv_init();

	// --- Timer de tick (5 ms) ---
	const esp_timer_create_args_t timerArgs = {
		.callback        = &tickIncCb,
		.arg             = nullptr,
		.dispatch_method = ESP_TIMER_TASK,
		.name            = "lvgl_tick",
		.skip_unhandled_events = false,
	};
	esp_timer_create(&timerArgs, &self->m_tickTimer);
	esp_timer_start_periodic(self->m_tickTimer, 5 * 1000); // 5 ms

	// --- Crear display LVGL ---
	self->m_lvglDisplay = lv_display_create(LCD_H_RES, LCD_V_RES);
	lv_display_set_user_data(self->m_lvglDisplay,
							 static_cast<void*>(self->m_panelHandle));
	lv_display_set_flush_cb(self->m_lvglDisplay, lvglFlushCb);

	// --- Buffer de renderizado en SRAM interna (DMA) ---
	const size_t drawBufSz = LCD_H_RES * DRAW_BUF_LINES * sizeof(lv_color_t);
	void* buf1 = heap_caps_malloc(drawBufSz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	configASSERT(buf1 != nullptr);
	lv_display_set_buffers(self->m_lvglDisplay, buf1, nullptr,
						   drawBufSz, LV_DISPLAY_RENDER_MODE_PARTIAL);

	// --- Registrar callback de fin de transferencia (ISR) ---
	esp_lcd_rgb_panel_event_callbacks_t cbs = {
		.on_color_trans_done = onTransferDone,
		.on_vsync            = nullptr,
		.on_bounce_empty     = nullptr,
	};
	esp_lcd_rgb_panel_register_event_callbacks(self->m_panelHandle,
											   &cbs,
											   self->m_lvglDisplay);

	// --- Registrar dispositivo de entrada táctil en LVGL ---
	self->m_lvglIndev = lv_indev_create();
	lv_indev_set_type(self->m_lvglIndev, LV_INDEV_TYPE_POINTER);
	lv_indev_set_user_data(self->m_lvglIndev, self);
	lv_indev_set_read_cb(self->m_lvglIndev, touchpadReadCb);

	ESP_LOGI(TAG_DM, "LVGL listo. Construyendo UI...");

	// --- Construir UI (sección protegida) ---
	if (self->m_uiReadyCb) {
		xSemaphoreTakeRecursive(self->m_guiSemaphore, portMAX_DELAY);
		self->m_uiReadyCb(self->m_lvglDisplay);
		xSemaphoreGiveRecursive(self->m_guiSemaphore);
	}

	// --- Loop principal de LVGL ---
	while (true) {
		vTaskDelay(pdMS_TO_TICKS(10));
		xSemaphoreTakeRecursive(self->m_guiSemaphore, portMAX_DELAY);
		lv_timer_handler();
		xSemaphoreGiveRecursive(self->m_guiSemaphore);
	}
}

/**
 * @brief Callback de LVGL para leer coordinadas táctiles.
 */
void DisplayManager::touchpadReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
	auto* self = static_cast<DisplayManager*>(lv_indev_get_user_data(indev));
	if (!self || !self->m_touchHandle) return;

	uint16_t touchX = 0;
	uint16_t touchY = 0;
	uint16_t touchStrength = 0;
	uint8_t touchCnt = 0;

	// Lee los datos reales del driver i2c de esp_lcd_touch
	esp_lcd_touch_read_data(self->m_touchHandle);

	// Obtiene coordenadas (soporta varios toques, usamos el 0)
	bool is_pressed = esp_lcd_touch_get_coordinates(self->m_touchHandle, &touchX, &touchY, &touchStrength, &touchCnt, 1);

	if (is_pressed && touchCnt > 0) {
		data->state = LV_INDEV_STATE_PRESSED;
		data->point.x = touchX;
		data->point.y = touchY;
	} else {
		data->state = LV_INDEV_STATE_RELEASED;
	}
}
