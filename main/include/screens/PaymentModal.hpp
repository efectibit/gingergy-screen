#pragma once
#include <functional>
#include "lvgl.h"
#include "../ChargePoint.hpp"

/**
 * @brief Modal de pago: QR binario + teclado numérico + validación de PIN (UC1.1).
 *
 * Flujo interno:
 *  1. show() → renderiza todo el modal y de manera asincrona tienes que pasarle el QR
 *  2. El usuario escanea el QR con su móvil y recibe un PIN
 *  3. El usuario ingresa el PIN con el teclado numérico en pantalla
 *  4. [VALIDATE PIN] → debe mandar al esclavo el PIN
 *  5. El esclavo responde si el PIN es válido
 *
 * Layout:
 *  ┌──────────────────────────────────────┐
 *  │  START CHARGING - TERMINAL 0X        │
 *  │  [QR binario]   ENTER PIN [XXXXXX]   │
 *  │  SCAN TO PAY        [7][8][9]        │
 *  │                     [4][5][6]        │
 *  │                     [1][2][3]        │
 *  │                     [  0 ][X]        │
 *  │     [ CANCEL ]    [ VALIDATE ]       │
 *  └──────────────────────────────────────┘
 */
class PaymentModal {
public:
	using PinValidateCallback = std::function<void(ChargePoint*, uint32_t)>;

	/**
	 * @param onValidate  Callback invocado tras enviar el PIN exitosamente.
	 */
	PaymentModal(PinValidateCallback onValidate);

	/**
	 * @brief Muestra el modal sobre la pantalla dada.
	 *        Genera el payload QR y lo renderiza.
	 * @param scr  Pantalla LVGL activa.
	 * @param cp   Punto de carga para el cual se genera el pago.
	 */
	void show(lv_obj_t* scr, ChargePoint* cp);

	/**
	 * @brief Oculta y destruye el modal.
	 */
	void hide();

	/**
	 * @brief Actualiza el QR binario.
	 */
	void updateQr(const uint8_t* payload, size_t len);

	/**
	 * @brief Actualiza el precio.
	 */
	void updatePrice(uint32_t priceRaw);

private:
	// --- Construcción de sub-secciones ---
	void buildQR(uint8_t* payload, size_t len);
	void buildNumpad();
	void buildPinDisplay();
	void showError(const char* msg);

	// --- Callbacks LVGL (static) ---
	static void onKeyPressedCb    (lv_event_t* e);
	static void onValidatePressedCb(lv_event_t* e);
	static void onCancelPressedCb(lv_event_t* e);
	static void onConfirmYesPressedCb(lv_event_t* e);
	static void onConfirmNoPressedCb(lv_event_t* e);

	// --- Helpers ---
	void showConfirmDialog();
	void appendPinChar(char c);
	void deletePinChar();
	void clearPin();
	void updatePinDisplay();

	// --- Widgets LVGL ---
	lv_obj_t* m_modal;        ///< Contenedor raíz del modal
	lv_obj_t* m_lblTitle;
	lv_obj_t* m_lblMinutes;   ///< Muestra los minutos ("120 minutos")
	lv_obj_t* m_lblPrice;
	lv_obj_t* m_qrCanvas;    ///< Canvas donde QRRenderer dibuja el QR
	lv_obj_t* m_lblPinField; ///< Muestra "* * * * * *" mientras el usuario escribe
	lv_obj_t* m_lblError;
	lv_obj_t* m_btnValidate;
	lv_obj_t* m_btnCancel;
	lv_obj_t* m_numpadMatrix;
	lv_obj_t* m_confirmMbox;        ///< MsgBox de confirmación (si está abierto)
	lv_obj_t* m_confirmMboxOverlay; ///< Overlay que bloquea el fondo

	static constexpr uint8_t PIN_MAX_LEN = 9;
	char    m_pinBuffer[PIN_MAX_LEN + 1];
	uint8_t m_pinLen;

	ChargePoint*          m_activePoint;
	PinValidateCallback m_onValidate;
};
