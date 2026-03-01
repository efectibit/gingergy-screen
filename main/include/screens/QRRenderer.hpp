#pragma once
#include <stdint.h>
#include <stddef.h>
#include "lvgl.h"

/**
 * @brief Renderiza un código QR binario sobre un lv_canvas de LVGL.
 *
 * Usa la biblioteca espressif/qrcode (qrcodegen_Mode_BYTE) para
 * codificar datos binarios arbitrarios y los dibuja pixel a pixel
 * escalados sobre un canvas LVGL.
 *
 * Uso típico:
 * @code
 *   lv_obj_t* canvas = QRRenderer::render(scr, payload, len, 20, 60, 3);
 * @endcode
 */
class QRRenderer {
public:
	/**
	 * @brief Genera y dibuja un QR directamente sobre la pantalla.
	 *
	 * @param parent   Objeto LVGL padre (pantalla o contenedor).
	 * @param data     Datos binarios a codificar (payload de CryptoPayment).
	 * @param dataLen  Longitud de los datos en bytes.
	 * @param x        Posición X del canvas dentro del padre.
	 * @param y        Posición Y del canvas dentro del padre.
	 * @param scale    Factor de escala: tamaño de cada módulo QR en píxeles.
	 *                 (ej. 3 → cada módulo ocupa 3×3 px)
	 * @return  Puntero al lv_obj_t (canvas) creado, o nullptr en caso de error.
	 */
	static lv_obj_t* render(lv_obj_t* parent,
							const uint8_t* data,
							size_t         dataLen,
							lv_coord_t     x,
							lv_coord_t     y,
							uint8_t        scale);

private:
	QRRenderer() = delete; // Clase puramente estática, no instanciable

	/**
	 * @brief Dibuja la matriz QR sobre el canvas ya creado.
	 * @param canvas     Canvas LVGL destino.
	 * @param qrMatrix   Matriz QR generada por qrcodegen.
	 * @param qrSize     Número de módulos por lado del QR.
	 * @param scale      Píxeles por módulo.
	 */
	static void drawOnCanvas(lv_obj_t*     canvas,
							 const uint8_t* qrMatrix,
							 int            qrSize,
							 uint8_t        scale);
};
