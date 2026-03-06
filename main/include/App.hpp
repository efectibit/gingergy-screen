#pragma once
#include <vector>
#include "DisplayManager.hpp"
#include "ControlBoardProxy.hpp"
#include "ChargePoint.hpp"
#include "screens/TerminalBar.hpp"
#include "screens/TimeSelectionScreen.hpp"
#include "screens/PaymentModal.hpp"

/**
 * @brief Orquestador principal de la aplicación.
 *
 * Responsabilidades:
 *  - Inicializar todos los subsistemas (display, Modbus)
 *  - Crear y conectar las pantallas/widgets con sus callbacks
 *  - Gestionar el flujo de navegación entre pantallas
 *  - Coordinar el ciclo de pago (QR → PIN → Modbus)
 *
 * Uso desde app_main():
 * @code
 *   App app(6);   // 6 puntos de carga
 *   app.run();    // Bloquea indefinidamente (loop FreeRTOS)
 * @endcode
 */
class App {
public:
	/**
	 * @param numPoints  Número de puntos de carga manejados por la
	 *                   placa controladora (1 a 6).
	 */
	explicit App(uint8_t numPoints);

	/**
	 * @brief Inicializa todos los subsistemas y entra al loop principal.
	 *        Esta función no retorna.
	 */
	void run();

private:
	// --- Callbacks de navegación (invocados desde las pantallas) ---

	/**
	 * @brief Llamado cuando el usuario toca un ícono de terminal.
	 *        Verifica disponibilidad y actualiza TimeSelectionScreen.
	 */
	void onTerminalSelected(uint8_t id);

	/**
	 * @brief Llamado cuando el usuario presiona [USAR].
	 *        Abre el PaymentModal.
	 */
	void onTimeConfirmed(ChargePoint* cp);

	/**
	 * @brief Llamado cuando el PIN es válido.
	 *        Envía el comando de carga por Modbus y cierra el modal.
	 */
	void onPaymentValidate(ChargePoint* cp, uint32_t pin);

	/**
	 * @brief Sincroniza el estado de los ChargePoints con la placa
	 *        controladora vía Modbus. Llamado periódicamente.
	 */
	void syncChargePointStatus();

	/**
	 * @brief Construye la interfaz gráfica. Invocado por DisplayManager
	 *        una vez que LVGL está listo (desde la tarea GUI en Core 1).
	 */
	void buildUI(lv_display_t* disp);

	// --- Subsistemas ---
	DisplayManager      m_display;
	ControlBoardProxy   m_proxy;

	// --- Modelo de dominio ---
	std::vector<ChargePoint> m_chargePoints;

	// --- Pantallas / Widgets ---
	TerminalBar*         m_terminalBar;
	TimeSelectionScreen* m_selectionScreen;
	PaymentModal*        m_paymentModal;
};
