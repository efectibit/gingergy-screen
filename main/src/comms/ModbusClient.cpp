#include "comms/ModbusClient.hpp"

ModbusClient::ModbusClient(uart_port_t uartNum,
						   uint8_t     slaveAddress,
						   int         txPin,
						   int         rxPin)
	: m_uart(uartNum)
	, m_slaveAddress(slaveAddress)
	, m_txPin(txPin)
	, m_rxPin(rxPin)
{}

esp_err_t ModbusClient::init() {
	return ESP_OK; // A implementar
}

esp_err_t ModbusClient::sendStartCharge(uint8_t terminalId, uint16_t minutes) {
	return ESP_OK; // A implementar
}

ChargePointStatus ModbusClient::readStatus(uint8_t terminalId) {
	return ChargePointStatus::AVAILABLE; // Stub
}

uint16_t ModbusClient::crc16(const uint8_t* buf, size_t len) {
	return 0; // A implementar
}

esp_err_t ModbusClient::transact(const uint8_t* request,  size_t reqLen,
								 uint8_t* response, size_t respMaxLen,
								 size_t&  respLen)
{
	return ESP_OK; // A implementar
}
