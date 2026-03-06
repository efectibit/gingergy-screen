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
	uint16_t work_status; // ChargeWorkMode
} input_terminal_status_response_t;

typedef struct {
	uint16_t terminal_id;    // 0x0000 (1 reg)
	uint32_t price;
	uint8_t  signature[64];
} input_terminal_price_response_t;

#pragma pack(pop)

// ============================================================================
// MACROS — Para cálculo dinámico de registros
// ============================================================================

// offsetof retorna bytes; cada registro Modbus es 2 bytes → shift derecho 1
// El "+ 1" es por el patrón del ejemplo de Espressif para evitar offset 0 en algunos casos
#define MB_OFFSET(struct_type, field) ((uint16_t)(offsetof(struct_type, field) + 1))

// Cantidad de registros que ocupa el campo = sizeof(campo) / 2
#define MB_REG_SIZE(struct_type, field)  (sizeof(((struct_type*)0)->field) >> 1)

// ============================================================================
// CIDs
// ============================================================================

enum {
	CID_HOLD_PRICE = 0,             // Holding: {terminal_id, req_minutes}
	CID_INPUT_TERMINAL_STATUS = 1,  // Input:   {terminal_id, work_mode}
	CID_INPUT_PRICE = 2,            // Input:   {terminal_id, signature[64], price}
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
	 * Lee el work mode del terminal.
	 */
	ChargeWorkMode readWorkMode(ChargePoint* cp);

	/**
	 * Lee el precio y la firma del terminal.
	 */
	esp_err_t readPrice(ChargePoint* cp);

	/**
	 * Solicita al esclavo la firma para el QR.
	 * Escribe {terminalId, minutes} en Holding → espera → lee Input completo.
	 */
	esp_err_t requestPrice(ChargePoint* cp);

	// Los siguientes métodos quedan pendientes de re-implementación con las nuevas estructuras
	// o se eliminarán si la lógica cambia a favor del polleo de estado.
	esp_err_t sendUserPin(ChargePoint* cp, uint32_t pin);
	esp_err_t setPointEnabled(ChargePoint* cp, bool enabled);
	esp_err_t setUnitPrice(ChargePoint* cp, uint32_t price);

private:
	uart_port_t m_uart;
	uint8_t     m_slaveAddr;
	void*       m_masterHandle;

	static const mb_parameter_descriptor_t m_deviceParameters[];
	static const size_t m_numParameters;

	// Buffers físicos granulares
	holding_terminal_price_request_t m_holdPriceReq;
	input_terminal_status_response_t m_statusResp;
	input_terminal_price_response_t  m_priceResp;
};

#endif // CONTROL_BOARD_PROXY_HPP
