#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Biblioteca portátil de pagos criptográficos.
 *
 * Sin dependencias de LVGL, ESP-IDF ni ningún otro framework.
 * Puede compilarse y testearse en host (PC) de forma independiente.
 *
 * Responsabilidades:
 *  - Firmar los datos de cobro (terminalId + minutes) y generar el
 *    payload binario que se codifica en el código QR.
 *  - Derivar el PIN de un solo uso a partir de los mismos datos.
 *  - Validar el PIN ingresado por el usuario contra la firma.
 *
 * Notas de implementación (reservadas para CryptoPayment.cpp):
 *  - El algoritmo y esquema de claves serán definidos por el desarrollador.
 *  - El payload es binario arbitrario; el QR debe codificarse en modo BYTE.
 */
class CryptoPayment {
public:
    CryptoPayment();

    /**
     * @brief Genera el payload binario que se codifica en el QR.
     *
     * El payload contiene los datos firmados criptográficamente:
     * terminalId y minutes. Su contenido exacto depende del algoritmo.
     *
     * @param terminalId  ID del punto de carga (1-based).
     * @param minutes     Tiempo de carga seleccionado (15..120).
     * @param outBuf      Buffer de salida para el payload binario.
     * @param outBufSize  Tamaño del buffer de salida en bytes.
     * @param outLen      Longitud real del payload generado (salida).
     * @return true si el payload fue generado con éxito.
     */
    bool generatePayload(uint8_t  terminalId,
                         uint16_t minutes,
                         uint8_t* outBuf,
                         size_t   outBufSize,
                         size_t&  outLen);

    /**
     * @brief Valida el PIN ingresado por el usuario.
     *
     * El PIN fue entregado al usuario por el sistema de pago externo
     * después de escanear el QR. Esta función verifica que el PIN
     * corresponde al mismo terminalId y minutes del payload generado.
     *
     * @param pin         PIN ingresado por el usuario.
     * @param terminalId  ID del punto de carga esperado.
     * @param minutes     Minutos de carga esperados.
     * @return true si el PIN es válido para los parámetros dados.
     */
    bool validatePIN(uint32_t pin,
                     uint8_t  terminalId,
                     uint16_t minutes);

private:
    // Los miembros privados (claves, estado interno) se definirán
    // en CryptoPayment.cpp cuando se implemente el algoritmo.
};
