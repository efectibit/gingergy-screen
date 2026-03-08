#pragma once
#include <functional>
#include "lvgl.h"
#include "../ChargePoint.hpp"

/**
 * @brief Pantalla principal de selección de tiempo de carga (UC1).
 *
 * Muestra:
 *  - Nombre del terminal activo
 *  - Arco semicircular que representa el tiempo seleccionado
 *  - Etiqueta de tiempo en formato H:MM y "X minutos"
 *  - Botones [−] y [+] para ajustar el tiempo (pasos de 15 min)
 *  - Botón [USAR] para confirmar y pasar al flujo de pago
 *  - Flechas laterales decorativas
 *
 * Los callbacks LVGL son static; recuperan `this` vía user_data.
 */
class TimeSelectionScreen {
public:
	using ConfirmCallback = std::function<void(ChargePoint*)>;

	/**
	 * @param onConfirm  Callback invocado cuando el usuario presiona [USAR].
	 *                   Recibe el puntero al ChargePoint activo.
	 */
	explicit TimeSelectionScreen(ConfirmCallback onConfirm);

	/**
	 * @brief Construye los widgets LVGL sobre la pantalla dada.
	 * @param scr  Pantalla LVGL activa.
	 */
	void build(lv_obj_t* scr);

	/**
	 * @brief Cambia el punto de carga activo y refresca la UI.
	 * @param cp  Puntero al ChargePoint seleccionado. Puede ser nullptr
	 *            para mostrar el estado sin terminal seleccionado.
	 */
	void setActivePoint(ChargePoint* cp);

	/**
	 * @brief Refresca etiquetas y arco según el estado del ChargePoint activo.
	 */
	void updateDisplay();

private:
	// --- Callbacks LVGL (static) ---
	static void onMinusPressedCb (lv_event_t* e);
	static void onPlusPressedCb  (lv_event_t* e);
	static void onConfirmPressedCb(lv_event_t* e);

	// --- Helpers ---
	/**
	 * @brief Convierte minutos al ángulo del arco (0° = 15 min, 180° = 120 min).
	 */
	uint16_t minutesToArcAngle(uint16_t minutes) const;

	/**
	 * @brief Formatea los minutos como "H:MM" (ej. 75 → "1:15").
	 */
	void formatTime(uint16_t minutes, char* buf, size_t bufSize) const;

	// --- Widgets LVGL ---
	lv_obj_t* m_lblTerminal;
	lv_obj_t* m_arc;
	lv_obj_t* m_lblTimeMain;   ///< "1:15"
	lv_obj_t* m_lblTimeSub;   ///< "75 minutos"
	lv_obj_t* m_btnMinus;
	lv_obj_t* m_btnPlus;
	lv_obj_t* m_btnConfirm;
	lv_obj_t* m_arrowLeft;
	lv_obj_t* m_arrowRight;

	ChargePoint*    m_activePoint;
	ConfirmCallback m_onConfirm;
};
