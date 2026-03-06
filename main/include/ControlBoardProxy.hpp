#ifndef CONTROL_BOARD_PROXY_HPP
#define CONTROL_BOARD_PROXY_HPP

#include "esp_err.h"
#include "driver/uart.h"
#include "mbcontroller.h" // ESP-Modbus controller
#include "ChargePoint.hpp"
#include <stddef.h>  // offsetof

// ============================================================================
// STRUCTS DE MEMORIA — Espejo del mapa de registros del esclavo
// Siguen el mismo patrón del ejemplo serial_master.c de ESP-IDF.
// Las macros HOLD_OFFSET / INPUT_OFFSET calculan las direcciones de forma
// automática usando offsetof, sin necesidad de valores hardcodeados.
// ============================================================================

#pragma pack(push, 1)

// "Terminal" refers to a charge point
// "Device" refers to the control board

// El envío del tiempo para que el esclavo genere firma y precio
typedef struct {
	uint16_t terminal_id;    // 0x0000 (1 reg) — SIEMPRE presente
	uint16_t req_minutes;    // 0x0001 (1 reg) — Solicita firma para N minutos
} holding_terminal_price_request_t;

typedef struct {
	uint16_t terminal_id;    // 0x0000 (1 reg)
	uint16_t work_mode; // ChargeWorkMode
} input_terminal_status_response_t;

typedef struct {
	uint16_t terminal_id;    // 0x0000 (1 reg)
	uint8_t  signature[64];
	uint32_t price;
} input_terminal_price_response_t;

#pragma pack(pop)

// ============================================================================
// MACROS — Idéntico patrón al ejemplo serial_master.c
// ============================================================================

// offsetof retorna bytes; cada registro Modbus es 2 bytes → shift derecho 1
#define HOLD_OFFSET(field)  ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))
#define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t,   field) + 1))

// Dirección inicial del registro = offset en bytes / 2
#define HOLD_REG_START(field)  (HOLD_OFFSET(field)  >> 1)
#define INPUT_REG_START(field) (INPUT_OFFSET(field) >> 1)

// Cantidad de registros que ocupa el campo = sizeof(campo) / 2
#define HOLD_REG_SIZE(field)  (sizeof(((holding_reg_params_t*)0)->field) >> 1)
#define INPUT_REG_SIZE(field) (sizeof(((input_reg_params_t*)0)->field)   >> 1)

// ============================================================================
// CIDs
// ============================================================================

enum {
	CID_HOLD_PRICE  = 0,  // Holding: {terminal_id, req_minutes} — Solicitar precio y esclavo internamente genera la firma
	CID_INPUT_TERMINAL_STATUS = 1,  // Input: {terminal_id, work_mode} — Solicitar estado del terminal
	CID_INPUT_PRICE     = 2,  // Input: {terminal_id, signature[64], price} — Solicitar precio y firma
	CID_COUNT
};

// ============================================================================
// CLASE
// ============================================================================

/**
 * @brief Proxy para la comunicación con la placa controladora.
 */
class ControlBoardProxy {
public:
	ControlBoardProxy(uart_port_t uartNum, uint8_t slaveAddr);
	~ControlBoardProxy();

	esp_err_t init(int txPin, int rxPin, uint32_t baudrate);
	esp_err_t inline start() { return mbc_master_start(m_masterHandle); }
	esp_err_t inline stop()  { return mbc_master_stop(m_masterHandle); }

	/**
	 * Solicita al esclavo la firma para el QR.
	 * Escribe {terminalId, minutes} en Holding → espera → lee Input completo.
	 * Rellena cp->setSignature() y cp->setPrice() con los datos del esclavo.
	 */
	esp_err_t requestSignature(ChargePoint* cp);

	/**
	 * Envía el PIN del usuario al esclavo para que lo valide.
	 * Si el PIN es correcto, el esclavo activa el relé internamente.
	 */
	esp_err_t sendUserPin(ChargePoint* cp, uint32_t pin);

	/**
	 * Lee el estado/energía/tiempo del terminal y actualiza el ChargePoint.
	 */
	esp_err_t syncStatus(ChargePoint* cp);

	/** Admin: habilita o deshabilita un punto de carga. */
	esp_err_t setPointEnabled(ChargePoint* cp, bool enabled);

	/** Admin: establece el precio por minuto global. */
	esp_err_t setUnitPrice(ChargePoint* cp, uint32_t price);

private:
	uart_port_t m_uart;
	uint8_t     m_slaveAddr;
	void*       m_masterHandle;

	static const mb_parameter_descriptor_t m_deviceParameters[];
	static const size_t m_numParameters;

	// Buffers físicos que el driver usa para leer/escribir en DMA
	input_reg_params_t   m_inputData;
	holding_reg_params_t m_holdingData;
};

#endif // CONTROL_BOARD_PROXY_HPP
