#pragma once
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "lvgl.h"

/**
 * @brief Gestiona la pantalla RGB y el ciclo de vida de LVGL.
 *
 * Responsabilidades:
 *  - Inicializa el panel LCD RGB (Sunton 7" 800×480)
 *  - Arranca la tarea FreeRTOS de LVGL en el Core 1
 *  - Expone lock()/unlock() para proteger el acceso a LVGL
 *    desde otras tareas
 *
 * Uso:
 * @code
 *   DisplayManager display;
 *   display.init();
 *   display.startGuiTask([](lv_display_t* disp) {
 *       // Construir UI aquí, LVGL ya está listo
 *   });
 * @endcode
 */
class DisplayManager {
public:
	DisplayManager();

	/**
	 * @brief Inicializa el panel físico RGB y los GPIOs.
	 *        Debe llamarse antes de startGuiTask().
	 */
	void init();

	/**
	 * @brief Crea la tarea FreeRTOS que ejecuta el loop de LVGL.
	 *
	 * Internamente:
	 *  1. Llama a lv_init() y configura el display LVGL
	 *  2. Registra los callbacks de flush y tick
	 *  3. Invoca uiReadyCb(display) \u2014 aquí se construye la UI
	 *  4. Entra al loop lv_timer_handler()
	 *
	 * @param uiReadyCb  Callback invocado una vez que LVGL está listo.
	 *                   Recibe el puntero al display LVGL.
	 */
	void startGuiTask(std::function<void(lv_display_t*)> uiReadyCb);

	/** @brief Toma el mutex de LVGL (bloquea hasta obtenerlo). */
	void lock();

	/** @brief Libera el mutex de LVGL. */
	void unlock();

	/** @return Puntero al display LVGL (válido después de startGuiTask()). */
	lv_display_t* getDisplay();

private:
	// --- Callbacks de bajo nivel (static, obligatorio para ESP-IDF / LVGL) ---
	static bool IRAM_ATTR onTransferDone(esp_lcd_panel_handle_t panel,
										 const esp_lcd_rgb_panel_event_data_t* data,
										 void* userCtx);
	static void lvglFlushCb(lv_display_t* disp,
							const lv_area_t* area,
							uint8_t* pxMap);
	static void tickIncCb(void* arg);
	static void guiTaskFn(void* param);   ///< param = DisplayManager*
	static void touchpadReadCb(lv_indev_t* indev, lv_indev_data_t* data);

	esp_lcd_panel_handle_t              m_panelHandle;
	esp_lcd_touch_handle_t              m_touchHandle;
	lv_display_t*                       m_lvglDisplay;
	lv_indev_t*                         m_lvglIndev;
	SemaphoreHandle_t                   m_guiSemaphore;
	esp_timer_handle_t                  m_tickTimer;
	std::function<void(lv_display_t*)>  m_uiReadyCb;
};
