#include "ControlBoardProxy.hpp"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "ControlBoardProxy";

// ============================================================================
// DICCIONARIO DE PARÁMETROS MODBUS
//
// Sigue exactamente el mismo patrón del ejemplo serial_master.c:
//  - Reg Start = HOLD_REG_START(campo) / INPUT_REG_START(campo)
//  - Reg Size  = HOLD_REG_SIZE(campo)  / INPUT_REG_SIZE(campo)
//  - Instance Offset = HOLD_OFFSET(campo) / INPUT_OFFSET(campo)
//
// Las macros calculan automáticamente las direcciones a partir de
// sizeof() y offsetof() sobre las structs definidas en el header.
// ============================================================================

const mb_parameter_descriptor_t ControlBoardProxy::m_deviceParameters[CID_COUNT] = {

    // CID_HOLD_PRICE — Enviar terminal_id + req_minutes
    {
        CID_HOLD_PRICE,
        (char*)"ReqMinutes", (char*)"min",
        1, MB_PARAM_HOLDING,
        0x0000, 2,                      // Reg Start = 0x0000, Size = 2 (id + min)
        MB_OFFSET(holding_terminal_price_request_t, terminal_id),
        PARAM_TYPE_ASCII,
        sizeof(holding_terminal_price_request_t),
        {0}, PAR_PERMS_READ_WRITE
    },

    // CID_INPUT_TERMINAL_STATUS — Leer terminal_id + work_mode
    {
        CID_INPUT_TERMINAL_STATUS,
        (char*)"TermStatus", (char*)"--",
        1, MB_PARAM_INPUT,
        0x0000, 2,                      // Reg Start = 0x0000, Size = 2 (id + mode)
        MB_OFFSET(input_terminal_status_response_t, terminal_id),
        PARAM_TYPE_ASCII,
        sizeof(input_terminal_status_response_t),
        {0}, PAR_PERMS_READ_WRITE
    },

    // CID_INPUT_PRICE — Leer terminal_id + signature + price
    {
        CID_INPUT_PRICE,
        (char*)"InputPrice", (char*)"--",
        1, MB_PARAM_INPUT,
        0x0002, 35,                     // Reg Start = 0x0002, Size = 35 (1 + 32 + 2)
        MB_OFFSET(input_terminal_price_response_t, terminal_id),
        PARAM_TYPE_ASCII,
        sizeof(input_terminal_price_response_t),
        {0}, PAR_PERMS_READ_WRITE
    },
};

const size_t ControlBoardProxy::m_numParameters = CID_COUNT;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ControlBoardProxy::ControlBoardProxy(uart_port_t uartNum, uint8_t slaveAddr)
    : m_uart(uartNum), m_slaveAddr(slaveAddr), m_masterHandle(nullptr) {
    memset(&m_holdPriceReq, 0, sizeof(m_holdPriceReq));
    memset(&m_statusResp,   0, sizeof(m_statusResp));
    memset(&m_priceResp,    0, sizeof(m_priceResp));
}

ControlBoardProxy::~ControlBoardProxy() {
    stop();
    if (m_masterHandle) {
        mbc_master_delete(m_masterHandle);
        m_masterHandle = nullptr;
    }
}

// ============================================================================
// INICIALIZACIÓN
// ============================================================================

esp_err_t ControlBoardProxy::init(int txPin, int rxPin, uint32_t baudrate) {
    mb_communication_info_t comm = {};
    comm.ser_opts.port             = m_uart;
    comm.ser_opts.mode             = MB_RTU;
    comm.ser_opts.baudrate         = baudrate;
    comm.ser_opts.parity           = MB_PARITY_NONE;
    comm.ser_opts.uid              = 0;
    comm.ser_opts.response_tout_ms = 1000;
    comm.ser_opts.data_bits        = UART_DATA_8_BITS;
    comm.ser_opts.stop_bits        = UART_STOP_BITS_1;

    esp_err_t err = mbc_master_create_serial(&comm, &m_masterHandle);
    if (err != ESP_OK) return err;

    err = uart_set_pin(m_uart, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    err = uart_set_mode(m_uart, UART_MODE_UART);
    if (err != ESP_OK) return err;

    err = mbc_master_set_descriptor(m_masterHandle, m_deviceParameters, m_numParameters);
    if (err != ESP_OK) return err;

    return ESP_OK;
}

// ============================================================================
// OPERACIONES
// ============================================================================

ChargeWorkMode ControlBoardProxy::readWorkMode(ChargePoint* cp) {
	if (!cp) return ChargeWorkMode::UNKNOWN;
	uint8_t type = 0;
	m_statusResp.terminal_id = cp->getId();
	esp_err_t err = mbc_master_get_parameter(m_masterHandle, CID_INPUT_TERMINAL_STATUS,
                                              (uint8_t*)&m_statusResp, &type);
	if (err != ESP_OK) {
        ESP_LOGE(TAG, "readWorkMode: error leyendo respuesta (T%d): %s",
				 cp->getId(), esp_err_to_name(err));
        return ChargeWorkMode::UNKNOWN;
    }
    
    ESP_LOGI(TAG, "T%d work mode (%u) recibido", cp->getId(), m_statusResp.work_status);
    return static_cast<ChargeWorkMode>(m_statusResp.work_status);
}

esp_err_t ControlBoardProxy::readPrice(ChargePoint* cp) {
	if (!cp) return ESP_ERR_INVALID_ARG;
	uint8_t type = 0;
	m_priceResp.terminal_id = cp->getId();
	esp_err_t err = mbc_master_get_parameter(m_masterHandle, CID_INPUT_PRICE,
                                              (uint8_t*)&m_priceResp, &type);
	if (err != ESP_OK) {
        ESP_LOGE(TAG, "readPrice: error leyendo respuesta (T%d): %s",
				 cp->getId(), esp_err_to_name(err));
        return err;
    }

    cp->setSignature(m_priceResp.signature);
    cp->setPrice(m_priceResp.price);
    
    ESP_LOGI(TAG, "T%d firma y precio (%lu) recibidos", cp->getId(), (unsigned long)m_priceResp.price);
    return ESP_OK;
}

esp_err_t ControlBoardProxy::requestPrice(ChargePoint* cp) {
    if (!cp) return ESP_ERR_INVALID_ARG;
    uint8_t type = 0;

    // 1. Escribir solicitud: {terminal_id, req_minutes}
    m_holdPriceReq.terminal_id = cp->getId();
    m_holdPriceReq.req_minutes = cp->getSelectedMinutes();
    
    esp_err_t err = mbc_master_set_parameter(m_masterHandle, CID_HOLD_PRICE,
                                               (uint8_t*)&m_holdPriceReq, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "requestPrice: error enviando solicitud (T%d): %s",
                 cp->getId(), esp_err_to_name(err));
        return err;
    }

	return ESP_OK;
}

esp_err_t ControlBoardProxy::sendUserPin(ChargePoint* cp, uint32_t pin) {
    // Pendiente de re-implementación según nuevo mapa Modbus
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t ControlBoardProxy::setPointEnabled(ChargePoint* cp, bool enabled) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t ControlBoardProxy::setUnitPrice(ChargePoint* cp, uint32_t price) {
    return ESP_ERR_NOT_SUPPORTED;
}
