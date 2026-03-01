#pragma once
#include <stdint.h>
#include "driver/uart.h"
#include "esp_err.h"
#include "../ChargePoint.hpp"

/**
 * @brief Cliente Modbus RTU sobre RS232 para comunicación con la
 *        placa controladora de carga.
 *
 * Protocolo: Modbus RTU, half-duplex, sobre UART (RS232).
 * La dirección de esclavo y el puerto UART se configuran en el constructor.
 *
 * Registros (acordar con firmware de la placa controladora):
 *  - Holding Register 0x0001 : Comando de inicio de carga
 *      bits [7:0]  → ID del terminal (1-based)
 *      bits [15:8] → Tiempo en minutos
 *  - Input Register 0x0001  : Estado del terminal (por ID)
 *      0x00 = AVAILABLE | 0x01 = OCCUPIED | 0x02 = FAULT | 0x03 = OUT_OF_SERVICE
 */
class ModbusClient {
public:
	/**
	 * @param uartNum       Puerto UART del ESP32 (ej. UART_NUM_1).
	 * @param slaveAddress  Dirección Modbus de la placa controladora.
	 * @param txPin         GPIO de transmisión (TX).
	 * @param rxPin         GPIO de recepción (RX).
	 */
	ModbusClient(uart_port_t uartNum,
				 uint8_t     slaveAddress,
				 int         txPin,
				 int         rxPin);

	/**
	 * @brief Inicializa el puerto UART y configura los parámetros Modbus.
	 * @return ESP_OK si la inicialización fue correcta.
	 */
	esp_err_t init();

	/**
	 * @brief Envía la instrucción de inicio de carga a la placa controladora.
	 * @param terminalId  ID del punto de carga (1-based).
	 * @param minutes     Tiempo de carga en minutos (15..120).
	 * @return ESP_OK si el comando fue enviado y reconocido (ACK Modbus).
	 */
	esp_err_t sendStartCharge(uint8_t terminalId, uint16_t minutes);

	/**
	 * @brief Lee el estado actual de un punto de carga desde la placa controladora.
	 * @param terminalId  ID del punto de carga (1-based).
	 * @return Estado del punto de carga.
	 */
	ChargePointStatus readStatus(uint8_t terminalId);

private:
	/**
	 * @brief Calcula el CRC16 Modbus de un buffer.
	 */
	static uint16_t crc16(const uint8_t* buf, size_t len);

	/**
	 * @brief Envía una trama Modbus RTU y espera la respuesta.
	 * @param request     Trama de solicitud (sin CRC, se agrega internamente).
	 * @param reqLen      Longitud de la solicitud.
	 * @param response    Buffer para la respuesta recibida.
	 * @param respMaxLen  Tamaño máximo del buffer de respuesta.
	 * @param respLen     Longitud real de la respuesta recibida (salida).
	 * @return ESP_OK si la trama fue enviada y se recibió respuesta válida.
	 */
	esp_err_t transact(const uint8_t* request,  size_t reqLen,
							 uint8_t* response, size_t respMaxLen,
							 size_t&  respLen);

	uart_port_t m_uart;
	uint8_t     m_slaveAddress;
	int         m_txPin;
	int         m_rxPin;
};
