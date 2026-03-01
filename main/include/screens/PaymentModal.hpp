#pragma once
#include <functional>
#include "lvgl.h"
#include "../ChargePoint.hpp"
#include "QRRenderer.hpp"

// Forward declaration para evitar inclusión circular
class CryptoPayment;

/**
 * @brief Modal de pago: QR binario + teclado numérico + validación de PIN (UC1.1).
 *
 * Flujo interno:
 *  1. show() → llama a CryptoPayment::generatePayload() y renderiza el QR
 *  2. El usuario escanea el QR con su móvil y recibe un PIN
 *  3. El usuario ingresa el PIN con el teclado numérico en pantalla
 *  4. [VALIDATE PIN] → llama a CryptoPayment::validatePIN()
 *     - Válido  → lanza onValidated(cp)
 *     - Inválido → muestra mensaje de error y permite reintentar
 *
 * Layout:
 *  ┌──────────────────────────────────────┐
 *  │  START CHARGING - TERMINAL XX        │
 *  │  [QR binario]   ENTER PIN [XXXXXX]   │
 *  │  SCAN TO PAY    [1][2][3]            │
 *  │                 [4][5][6]            │
 *  │                 [7][8][DEL]          │
 *  │                 [*][0][OK]           │
 *  │         [   VALIDATE PIN   ]         │
 *  └──────────────────────────────────────┘
 */
class PaymentModal {
public:
	/**
	 * @param crypto       Puntero a la instancia de CryptoPayment.
	 * @param onValidated  Callback invocado si el PIN es correcto.
	 *                     Recibe el puntero al ChargePoint activo.
	 */
	PaymentModal(CryptoPayment* crypto,
				 std::function<void(ChargePoint*)> onValidated);

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

private:
	// --- Construcción de sub-secciones ---
	void buildQR(uint8_t* payload, size_t len);
	void buildNumpad();
	void buildPinDisplay();
	void showError(const char* msg);

	// --- Callbacks LVGL (static) ---
	static void onKeyPressedCb    (lv_event_t* e);
	static void onValidatePressedCb(lv_event_t* e);

	// --- Helpers ---
	void appendPinChar(char c);
	void deletePinChar();
	void clearPin();
	void updatePinDisplay();

	// --- Widgets LVGL ---
	lv_obj_t* m_modal;        ///< Contenedor raíz del modal
	lv_obj_t* m_lblTitle;
	lv_obj_t* m_lblPrice;
	lv_obj_t* m_qrCanvas;    ///< Canvas donde QRRenderer dibuja el QR
	lv_obj_t* m_lblPinField; ///< Muestra "* * * * * *" mientras el usuario escribe
	lv_obj_t* m_lblError;
	lv_obj_t* m_btnValidate;
	lv_obj_t* m_numpadBtns[12];

	static constexpr uint8_t PIN_MAX_LEN = 6;
	char    m_pinBuffer[PIN_MAX_LEN + 1];
	uint8_t m_pinLen;

	ChargePoint*                      m_activePoint;
	CryptoPayment*                    m_crypto;
	std::function<void(ChargePoint*)> m_onValidated;
};
