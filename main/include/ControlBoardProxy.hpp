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

/** Input Registers: datos que el maestro lee del esclavo.
 *  El terminal_id siempre encabeza el struct para que el esclavo sepa
 *  a qué punto de carga responder. */
typedef struct {
	uint16_t terminal_id;    // 0x0000 (1 reg)
	uint8_t  signature[64];  // 0x0001–0x0020 (32 reg) — Firma de 64 bytes del esclavo
	uint32_t price;          // 0x0021–0x0022 (2 reg)  — Precio con 3 dec. implícitos
	uint16_t status;         // 0x0023 (1 reg) — 0=Disp, 1=Ocup, 2=Falla, 3=FueraSrv
	uint16_t elapsed_time;   // 0x0024 (1 reg) — Minutos transcurridos en la sesión
	uint16_t energy;         // 0x0025 (1 reg) — Watts consumidos en la sesión
} input_reg_params_t;        // Total: 1+32+2+1+1+1 = 38 registros

/** Holding Registers: commandos/parámetros que el maestro escribe al esclavo.
 *  terminal_id siempre va primero. */
typedef struct {
	uint16_t terminal_id;    // 0x0000 (1 reg) — SIEMPRE presente
	uint16_t req_minutes;    // 0x0001 (1 reg) — Solicita firma para N minutos
	uint32_t user_pin;       // 0x0002–0x0003 (2 reg) — PIN hasta 8 dígitos
	uint16_t enable_point;   // 0x0004 (1 reg) — 1=Habilitar, 0=Deshabilitar (admin)
	uint32_t unit_price;     // 0x0005–0x0006 (2 reg) — Precio/min establecido por admin
} holding_reg_params_t;      // Total: 1+1+2+1+2 = 7 registros

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
	CID_INPUT_QUERY  = 0,  // Input:   {terminal_id, signature[64], price, status, elapsed, energy}
	CID_HOLD_REQUEST = 1,  // Holding: {terminal_id, req_minutes, ...} — Solicitar firma
	CID_HOLD_PIN     = 2,  // Holding: {terminal_id, user_pin}         — Enviar PIN
	CID_HOLD_ENABLE  = 3,  // Holding: {terminal_id, enable_point}     — Admin: habilitar
	CID_HOLD_PRICE   = 4,  // Holding: {terminal_id, unit_price}       — Admin: precio
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
