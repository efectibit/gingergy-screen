#include "screens/QRRenderer.hpp"
#include <qrcode.h>
#include "esp_log.h"
#include <vector>

static const char* TAG = "QRRenderer";

// Variables estáticas para pasar estado al callback de espressif/qrcode
static lv_obj_t*  g_current_parent = nullptr;
static lv_coord_t g_current_x = 0;
static lv_coord_t g_current_y = 0;
static uint8_t    g_current_scale = 1;
static lv_obj_t*  g_created_canvas = nullptr;

/**
 * Callback sincrónico disparado dentro de esp_qrcode_generate().
 * Aquí extraemos el tamaño final del QR y dibujamos la matriz.
 */
static void drawOnCanvasCb(esp_qrcode_handle_t qrcode) {
	if (!g_current_parent) return;

	int qrSize = esp_qrcode_get_size(qrcode);
	int canvasSize = qrSize * g_current_scale;

	g_created_canvas = lv_canvas_create(g_current_parent);

	// Asignar memoria para el buffer del Canvas (formato RGB565)
	size_t cbuf_size = lv_draw_buf_width_to_stride(canvasSize, LV_COLOR_FORMAT_RGB565) * canvasSize;
	void* cbuf = lv_malloc_zeroed(cbuf_size);
	if (!cbuf) {
		ESP_LOGE(TAG, "Failed to allocate canvas buffer");
		lv_obj_delete(g_created_canvas);
		g_created_canvas = nullptr;
		return;
	}

	lv_canvas_set_buffer(g_created_canvas, cbuf, canvasSize, canvasSize, LV_COLOR_FORMAT_RGB565);
	lv_canvas_fill_bg(g_created_canvas, lv_color_white(), LV_OPA_COVER);
	lv_obj_set_pos(g_created_canvas, g_current_x, g_current_y);

	lv_layer_t layer;
	lv_canvas_init_layer(g_created_canvas, &layer);

	lv_draw_rect_dsc_t rect_dsc;
	lv_draw_rect_dsc_init(&rect_dsc);
	rect_dsc.bg_color = lv_color_black();
	rect_dsc.bg_opa = LV_OPA_COVER;

	// Recorrer matriz y dibujar módulos
	for (int y = 0; y < qrSize; y++) {
		for (int x = 0; x < qrSize; x++) {
			if (esp_qrcode_get_module(qrcode, x, y)) {
				lv_area_t a;
				a.x1 = x * g_current_scale;
				a.y1 = y * g_current_scale;
				a.x2 = a.x1 + g_current_scale - 1;
				a.y2 = a.y1 + g_current_scale - 1;
				lv_draw_rect(&layer, &rect_dsc, &a);
			}
		}
	}
	lv_canvas_finish_layer(g_created_canvas, &layer);
}

/**
 * Función auxiliar para convertir binario en cadena hexadecimal.
 * Necesaria porque esp_qrcode_generate solo acepta const char*
 */
static void bytesToHexString(const uint8_t* in, size_t inLen, std::vector<char>& outStr) {
	const char hexChars[] = "0123456789ABCDEF";
	for (size_t i = 0; i < inLen; i++) {
		outStr[i * 2]     = hexChars[(in[i] >> 4) & 0x0F];
		outStr[i * 2 + 1] = hexChars[in[i] & 0x0F];
	}
	outStr[inLen * 2] = '\0';
}

lv_obj_t* QRRenderer::render(lv_obj_t* parent,
							 const uint8_t* data,
							 size_t         dataLen,
							 lv_coord_t     x,
							 lv_coord_t     y,
							 uint8_t        scale)
{
	if (!data || dataLen == 0 || !parent) return nullptr;

	// Transformamos los datos binarios en texto hexadecimal superior.
	std::vector<char> hexStr(dataLen * 2 + 1);
	bytesToHexString(data, dataLen, hexStr);

	// Preparar estado estático para que lo lea el callback
	g_current_parent = parent;
	g_current_x = x;
	g_current_y = y;
	g_current_scale = scale;
	g_created_canvas = nullptr;

	esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
	cfg.display_func = drawOnCanvasCb;
	cfg.max_qrcode_version = 15; // Versión alta para admitir cadenas en formato hexadecimal
	cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;

	esp_err_t err = esp_qrcode_generate(&cfg, hexStr.data());

	// Limpieza
	g_current_parent = nullptr;

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_qrcode_generate falló con código: %s", esp_err_to_name(err));
		return nullptr;
	}

	return g_created_canvas;
}

void QRRenderer::drawOnCanvas(lv_obj_t* canvas, const uint8_t* qrMatrix, int qrSize, uint8_t scale)
{
	// Función sin uso cuando se emite a través del callback nativo de espressif/qrcode.
}
