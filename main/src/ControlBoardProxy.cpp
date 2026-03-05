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

    // CID_INPUT_QUERY — Bloque completo de lectura del esclavo
    // El master escribe terminal_id en los primeros 2 bytes antes de leer.
    // El esclavo responde con signature[64] + price + status + elapsed + energy.
    {
        CID_INPUT_QUERY,
        (char*)"InputBlock", (char*)"--",
        1,                              // Slave address
        MB_PARAM_INPUT,
        INPUT_REG_START(terminal_id),  // Reg Start = 0x0000
        INPUT_REG_SIZE(terminal_id)    // Solo 1 registro para escribir el ID antes de leer
            + INPUT_REG_SIZE(signature)
            + INPUT_REG_SIZE(price)
            + INPUT_REG_SIZE(status)
            + INPUT_REG_SIZE(elapsed_time)
            + INPUT_REG_SIZE(energy),  // Reg Size  = 38 en total
        INPUT_OFFSET(terminal_id),     // Instance Offset apunta al inicio de la struct
        PARAM_TYPE_ASCII,              // Tipo genérico: bloque de bytes sin interpretación
        sizeof(input_reg_params_t),    // Data Size = 76 bytes
        {0}, PAR_PERMS_READ_WRITE
    },

    // CID_HOLD_REQUEST — Solicitar firma: {terminal_id, req_minutes}
    {
        CID_HOLD_REQUEST,
        (char*)"ReqMinutes", (char*)"min",
        1, MB_PARAM_HOLDING,
        HOLD_REG_START(terminal_id),   // 0x0000
        HOLD_REG_SIZE(terminal_id) + HOLD_REG_SIZE(req_minutes),  // 2 registros
        HOLD_OFFSET(terminal_id),
        PARAM_TYPE_ASCII,
        sizeof(uint16_t) * 2,
        {0}, PAR_PERMS_READ_WRITE
    },

    // CID_HOLD_PIN — Enviar PIN: {terminal_id, user_pin}
    {
        CID_HOLD_PIN,
        (char*)"UserPin", (char*)"pwd",
        1, MB_PARAM_HOLDING,
        HOLD_REG_START(user_pin),      // 0x0002
        HOLD_REG_SIZE(terminal_id) + HOLD_REG_SIZE(user_pin),     // 3 registros
        HOLD_OFFSET(terminal_id),
        PARAM_TYPE_ASCII,
        sizeof(uint16_t) + sizeof(uint32_t),
        {0}, PAR_PERMS_READ_WRITE
    },

    // CID_HOLD_ENABLE — Admin: habilitar/deshabilitar {terminal_id, enable_point}
    {
        CID_HOLD_ENABLE,
        (char*)"EnablePoint", (char*)"bool",
        1, MB_PARAM_HOLDING,
        HOLD_REG_START(enable_point),  // 0x0004
        HOLD_REG_SIZE(terminal_id) + HOLD_REG_SIZE(enable_point), // 2 registros
        HOLD_OFFSET(terminal_id),
        PARAM_TYPE_ASCII,
        sizeof(uint16_t) * 2,
        {0}, PAR_PERMS_READ_WRITE
    },

    // CID_HOLD_PRICE — Admin: precio por minuto {terminal_id, unit_price}
    {
        CID_HOLD_PRICE,
        (char*)"UnitPrice", (char*)"$/min",
        1, MB_PARAM_HOLDING,
        HOLD_REG_START(unit_price),    // 0x0005
        HOLD_REG_SIZE(terminal_id) + HOLD_REG_SIZE(unit_price),  // 3 registros
        HOLD_OFFSET(terminal_id),
        PARAM_TYPE_ASCII,
        sizeof(uint16_t) + sizeof(uint32_t),
        {0}, PAR_PERMS_READ_WRITE
    },
};

const size_t ControlBoardProxy::m_numParameters = CID_COUNT;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ControlBoardProxy::ControlBoardProxy(uart_port_t uartNum, uint8_t slaveAddr)
    : m_uart(uartNum), m_slaveAddr(slaveAddr), m_masterHandle(nullptr) {
    memset(&m_inputData,   0, sizeof(m_inputData));
    memset(&m_holdingData, 0, sizeof(m_holdingData));
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

esp_err_t ControlBoardProxy::requestSignature(ChargePoint* cp) {
    if (!cp) return ESP_ERR_INVALID_ARG;
    uint8_t type = 0;

    // 1. Preparar y escribir solicitud: {terminal_id, req_minutes}
    m_holdingData.terminal_id  = cp->getId();
    m_holdingData.req_minutes  = cp->getSelectedMinutes();
    esp_err_t err = mbc_master_set_parameter(m_masterHandle, CID_HOLD_REQUEST,
                                              (uint8_t*)&m_holdingData, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "requestSignature: error enviando solicitud (T%d): %s",
                 cp->getId(), esp_err_to_name(err));
        return err;
    }

    // 2. Espera corta para que el esclavo genere la firma
    vTaskDelay(pdMS_TO_TICKS(50));

    // 3. Leer bloque completo de Input Registers
    m_inputData.terminal_id = cp->getId();
    err = mbc_master_get_parameter(m_masterHandle, CID_INPUT_QUERY,
                                   (uint8_t*)&m_inputData, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "requestSignature: error leyendo respuesta (T%d): %s",
                 cp->getId(), esp_err_to_name(err));
        return err;
    }

    // 4. Poblar el ChargePoint con los datos del esclavo
    cp->setSignature(m_inputData.signature);
    cp->setPrice(m_inputData.price);
    ESP_LOGI(TAG, "T%d firma recibida, precio: %lu", cp->getId(), (unsigned long)m_inputData.price);
    return ESP_OK;
}

esp_err_t ControlBoardProxy::sendUserPin(ChargePoint* cp, uint32_t pin) {
    if (!cp) return ESP_ERR_INVALID_ARG;
    uint8_t type = 0;

    m_holdingData.terminal_id = cp->getId();
    m_holdingData.user_pin    = pin;
    esp_err_t err = mbc_master_set_parameter(m_masterHandle, CID_HOLD_PIN,
                                              (uint8_t*)&m_holdingData, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sendUserPin: error (T%d): %s", cp->getId(), esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "T%d PIN enviado al esclavo", cp->getId());
    }
    return err;
}

esp_err_t ControlBoardProxy::syncStatus(ChargePoint* cp) {
    if (!cp) return ESP_ERR_INVALID_ARG;
    uint8_t type = 0;

    m_inputData.terminal_id = cp->getId();
    esp_err_t err = mbc_master_get_parameter(m_masterHandle, CID_INPUT_QUERY,
                                              (uint8_t*)&m_inputData, &type);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "syncStatus: timeout (T%d)", cp->getId());
        return err;
    }

    // Mapear el valor numérico al enum
    uint16_t v = m_inputData.status;
    cp->setStatus(v <= 3 ? static_cast<ChargePointStatus>(v) : ChargePointStatus::FAULT);
    cp->setEnergy(m_inputData.energy);
    cp->setElapsedTime(m_inputData.elapsed_time);
    return ESP_OK;
}

esp_err_t ControlBoardProxy::setPointEnabled(ChargePoint* cp, bool enabled) {
    if (!cp) return ESP_ERR_INVALID_ARG;
    uint8_t type = 0;

    m_holdingData.terminal_id   = cp->getId();
    m_holdingData.enable_point  = enabled ? 1 : 0;
    esp_err_t err = mbc_master_set_parameter(m_masterHandle, CID_HOLD_ENABLE,
                                              (uint8_t*)&m_holdingData, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setPointEnabled: error (T%d, en=%d): %s",
                 cp->getId(), (int)enabled, esp_err_to_name(err));
    }
    return err;
}

esp_err_t ControlBoardProxy::setUnitPrice(ChargePoint* cp, uint32_t price) {
    if (!cp) return ESP_ERR_INVALID_ARG;
    uint8_t type = 0;

    m_holdingData.terminal_id = cp->getId();
    m_holdingData.unit_price  = price;
    esp_err_t err = mbc_master_set_parameter(m_masterHandle, CID_HOLD_PRICE,
                                              (uint8_t*)&m_holdingData, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setUnitPrice: error (T%d): %s", cp->getId(), esp_err_to_name(err));
    }
    return err;
}
