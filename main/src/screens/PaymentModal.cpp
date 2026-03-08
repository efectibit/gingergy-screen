#include "screens/PaymentModal.hpp"
#include <stdio.h>
#include <string.h>

PaymentModal::PaymentModal(PinValidateCallback onValidate)
	: m_modal(nullptr)
	, m_lblTitle(nullptr)
	, m_lblMinutes(nullptr)
	, m_lblPrice(nullptr)
	, m_qrCanvas(nullptr)
	, m_lblPinField(nullptr)
	, m_lblError(nullptr)
	, m_btnValidate(nullptr)
	, m_btnCancel(nullptr)
	, m_numpadMatrix(nullptr)
	, m_confirmMbox(nullptr)
	, m_confirmMboxOverlay(nullptr)
	, m_wrongPinMbox(nullptr)
	, m_wrongPinMboxOverlay(nullptr)
	, m_pinLen(0)
	, m_activePoint(nullptr)
	, m_onValidate(onValidate)
{
	m_pinBuffer[0] = '\0';
}

void PaymentModal::show(lv_obj_t* scr, ChargePoint* cp) {
	if (!cp) return;
	m_activePoint = cp;

	// Fondo oscurecido
	m_modal = lv_obj_create(scr);
	lv_obj_set_size(m_modal, 800, 480);
	lv_obj_center(m_modal);
	lv_obj_set_style_radius(m_modal, 0, 0); // no rounded corners
	lv_obj_set_style_border_width(m_modal, 0, 0); // no border
	lv_obj_set_style_shadow_width(m_modal, 0, 0); // no shadow
	lv_obj_set_style_bg_color(m_modal, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(m_modal, LV_OPA_70, 0);
	lv_obj_clear_flag(m_modal, LV_OBJ_FLAG_SCROLLABLE);

	// Contenedor blanco central
	lv_obj_t* box = lv_obj_create(m_modal);
	lv_obj_set_size(box, 600, 420);
	lv_obj_center(box);
	lv_obj_set_style_bg_color(box, lv_color_white(), 0);
	lv_obj_set_style_radius(box, 15, 0);
	lv_obj_set_style_border_color(box, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
	lv_obj_set_style_border_width(box, 4, 0);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

	// --- Parte Superior: Título ---
	m_lblTitle = lv_label_create(box);
	lv_label_set_text_fmt(m_lblTitle, "START CHARGING - TERMINAL %02d", cp->getId());
	lv_obj_set_style_text_font(m_lblTitle, &lv_font_montserrat_22, 0);
	lv_obj_align(m_lblTitle, LV_ALIGN_TOP_MID, 0, 10);

	// --- Columna Izquierda: Información y QR ---
	m_lblMinutes = lv_label_create(box);
	lv_label_set_text_fmt(m_lblMinutes, "%d minutos", cp->getSelectedMinutes());
	lv_obj_set_style_text_font(m_lblMinutes, &lv_font_montserrat_22, 0);
	lv_obj_align(m_lblMinutes, LV_ALIGN_TOP_LEFT, 50, 60);

	// Precio viene del esclavo (2 decimales implícitos), o estimación local si aún no hay firma
	uint32_t priceRaw = cp->getPrice();
	m_lblPrice = lv_label_create(box);
	if (priceRaw > 0) {
		// Formatear precio con 2 decimales: 12345 → "123.45"
		lv_label_set_text_fmt(m_lblPrice, "S/ %lu.%02lu", (unsigned long)(priceRaw / 100), (unsigned long)(priceRaw % 100));
	} else {
		// Aún no se ha solicitado la firma al esclavo
		lv_label_set_text(m_lblPrice, "S/ ---");
	}
	lv_obj_set_style_text_font(m_lblPrice, &lv_font_montserrat_32, 0);
	lv_obj_align_to(m_lblPrice, m_lblMinutes, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

	// Generar Payload y QR
	lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
	lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);

	m_qrCanvas = lv_qrcode_create(box);
	lv_qrcode_set_size(m_qrCanvas, 150);
	lv_qrcode_set_dark_color(m_qrCanvas, fg_color);
	lv_qrcode_set_light_color(m_qrCanvas, bg_color);

	/*Set QR data*/
	const uint8_t data[] = {0, 1, 2, 3, 4, 5, 6, 7};
	lv_qrcode_update(m_qrCanvas, data, sizeof(data));
	lv_obj_align(m_qrCanvas, LV_ALIGN_LEFT_MID, 50, 14);

	/*Add a border with bg_color*/
	lv_obj_set_style_border_color(m_qrCanvas, lv_color_black(), 0);
	lv_obj_set_style_border_width(m_qrCanvas, 5, 0);

	// Label bajo el QR
	lv_obj_t* lblScan = lv_label_create(box);
	lv_label_set_text(lblScan, "SCAN TO PAY");
	lv_obj_set_style_text_font(lblScan, &lv_font_montserrat_16, 0);
	lv_obj_align_to(lblScan, m_qrCanvas, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

	// --- Columna Derecha: Teclado y PIN ---
	lv_obj_t* lblEnterPin = lv_label_create(box);
	lv_label_set_text(lblEnterPin, "ENTER PIN:");
	lv_obj_set_style_text_font(lblEnterPin, &lv_font_montserrat_20, 0);
	lv_obj_align(lblEnterPin, LV_ALIGN_TOP_RIGHT, -120, 50);

	m_lblPinField = lv_label_create(box);
	lv_label_set_text(m_lblPinField, "");
	lv_obj_set_size(m_lblPinField, 220, 40);
	lv_obj_set_style_bg_color(m_lblPinField, lv_color_hex(0xB2EBF2), 0); // Cian claro
	lv_obj_set_style_bg_opa(m_lblPinField, LV_OPA_COVER, 0);
	lv_obj_set_style_text_align(m_lblPinField, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(m_lblPinField, &lv_font_montserrat_24, 0);
	lv_obj_set_style_border_color(m_lblPinField, lv_color_black(), 0);
	lv_obj_set_style_border_width(m_lblPinField, 1, 0);
	// Para alinear verticalmente el texto en LVGL se suele usar padding o flex, usaremos padding temporal
	lv_obj_set_style_pad_top(m_lblPinField, 5, 0);
	lv_obj_align_to(m_lblPinField, lblEnterPin, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

	// Construir la matriz de botones
	buildNumpad();
	lv_obj_align_to(m_numpadMatrix, m_lblPinField, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

	// Error label (oculto por defecto)
	m_lblError = lv_label_create(box);
	lv_label_set_text(m_lblError, "");
	lv_obj_set_style_text_color(m_lblError, lv_palette_main(LV_PALETTE_RED), 0);
	// Alineamos cerca del bottom
	lv_obj_align(m_lblError, LV_ALIGN_BOTTOM_MID, 0, -80);

	// --- Parte Inferior: Botones CANCELAR y VALIDAR ---
	m_btnCancel = lv_button_create(box);
	lv_obj_set_size(m_btnCancel, 200, 50);
	lv_obj_align(m_btnCancel, LV_ALIGN_BOTTOM_LEFT, 40, -10);
	lv_obj_set_style_bg_color(m_btnCancel, lv_color_hex(0xE0E0E0), 0);
	lv_obj_set_style_border_width(m_btnCancel, 2, 0);
	lv_obj_set_style_border_color(m_btnCancel, lv_color_black(), 0);
	lv_obj_set_style_radius(m_btnCancel, 0, 0);

	lv_obj_t* lblCancel = lv_label_create(m_btnCancel);
	lv_label_set_text(lblCancel, "CANCELAR");
	lv_obj_set_style_text_font(lblCancel, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(lblCancel, lv_color_black(), 0);
	lv_obj_center(lblCancel);
	lv_obj_add_event_cb(m_btnCancel, onCancelPressedCb, LV_EVENT_CLICKED, this);

	m_btnValidate = lv_button_create(box);
	lv_obj_set_size(m_btnValidate, 200, 50);
	lv_obj_align(m_btnValidate, LV_ALIGN_BOTTOM_RIGHT, -40, -10);
	lv_obj_set_style_bg_color(m_btnValidate, lv_color_hex(0xA3D9A5), 0);
	lv_obj_set_style_border_width(m_btnValidate, 2, 0);
	lv_obj_set_style_border_color(m_btnValidate, lv_color_black(), 0);
	lv_obj_set_style_radius(m_btnValidate, 0, 0);

	lv_obj_t* lblVal = lv_label_create(m_btnValidate);
	lv_label_set_text(lblVal, "VALIDAR");
	lv_obj_set_style_text_font(lblVal, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(lblVal, lv_color_black(), 0);
	lv_obj_center(lblVal);
	lv_obj_add_event_cb(m_btnValidate, onValidatePressedCb, LV_EVENT_CLICKED, this);

	clearPin();
}

void PaymentModal::hide() {
	if (m_modal) {
		lv_obj_delete(m_modal);
		m_modal = nullptr;
	}
	m_activePoint = nullptr;
	m_lblTitle = nullptr;
	m_lblMinutes = nullptr;
	m_lblPrice = nullptr;
	m_qrCanvas = nullptr;
	m_lblPinField = nullptr;
	m_lblError = nullptr;
	m_btnValidate = nullptr;
	m_btnCancel = nullptr;
	m_numpadMatrix = nullptr;
	m_confirmMbox = nullptr;
	m_confirmMboxOverlay = nullptr;
	m_wrongPinMbox = nullptr;
	m_wrongPinMboxOverlay = nullptr;

	clearPin();
}

void PaymentModal::updateQr(const uint8_t* payload, size_t len) {
	if (m_qrCanvas) {
		lv_qrcode_update(m_qrCanvas, payload, len);
	}
}

void PaymentModal::updatePrice(uint32_t priceRaw) {
	if (this->m_lblPrice) {
		lv_label_set_text_fmt(this->m_lblPrice, "S/ %lu.%02lu", (unsigned long)(priceRaw / 100), (unsigned long)(priceRaw % 100));
	}
}

static const char * btnm_map[] = {
	"7", "8", "9", "\n",
	"4", "5", "6", "\n",
	"1", "2", "3", "\n",
	"0", LV_SYMBOL_BACKSPACE, ""
};

void PaymentModal::buildNumpad() {
	lv_obj_t* parent = lv_obj_get_child(m_modal, 0); // box

	m_numpadMatrix = lv_buttonmatrix_create(parent);
	lv_buttonmatrix_set_map(m_numpadMatrix, btnm_map);

	// Configuración según el código de ejemplo del usuario
	// Hacer el "0" el doble de ancho que "backspace" (están en la fila 3, botones 0 y 1)
	// Map = fila 0: 0,1,2 | fila 1: 3,4,5 | fila 2: 6,7,8 | fila 3: 9,10.
	lv_buttonmatrix_set_button_width(m_numpadMatrix, 9, 2); // Botón 9 es el "0"

	// Estilizar matriz para que parezca blanca con bordes
	lv_obj_set_style_bg_opa(m_numpadMatrix, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(m_numpadMatrix, 0, LV_PART_MAIN);

	// Ajustes de tamaño y botones
	lv_obj_set_size(m_numpadMatrix, 240, 200);

	// Estilos de los botones
	lv_obj_set_style_bg_color(m_numpadMatrix, lv_color_white(), LV_PART_ITEMS);
	lv_obj_set_style_border_color(m_numpadMatrix, lv_color_black(), LV_PART_ITEMS);
	lv_obj_set_style_border_width(m_numpadMatrix, 1, LV_PART_ITEMS);
	lv_obj_set_style_text_color(m_numpadMatrix, lv_color_black(), LV_PART_ITEMS);
	lv_obj_set_style_text_font(m_numpadMatrix, &lv_font_montserrat_32, LV_PART_ITEMS);

	lv_obj_add_event_cb(m_numpadMatrix, onKeyPressedCb, LV_EVENT_VALUE_CHANGED, this);
}

void PaymentModal::onKeyPressedCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));
	lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);

	uint32_t id = lv_buttonmatrix_get_selected_button(obj);
	const char* txt = lv_buttonmatrix_get_button_text(obj, id);

	if (!txt) return;

	if (txt[0] >= '0' && txt[0] <= '9') {
		self->appendPinChar(txt[0]);
	} else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
		self->deletePinChar();
	}
}

void PaymentModal::onValidatePressedCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));
	if (!self->m_activePoint || self->m_pinLen == 0) return;

	uint32_t pin = 0;
	sscanf(self->m_pinBuffer, "%lu", &pin);

	self->m_onValidate(self->m_activePoint, pin);
}

void PaymentModal::onCancelPressedCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));
	self->showConfirmDialog();
}

void PaymentModal::showConfirmDialog() {
	if (m_confirmMbox) return; // Ya existe uno abierto, no hacemos nada

	// Creamos un overlay transparente que ocupe todo m_modal para bloquear clics de fondo
	m_confirmMboxOverlay = lv_obj_create(m_modal);
	lv_obj_set_size(m_confirmMboxOverlay, 600, 420);
	lv_obj_set_style_bg_color(m_confirmMboxOverlay, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(m_confirmMboxOverlay, LV_OPA_70, 0);
	lv_obj_set_style_border_width(m_confirmMboxOverlay, 0, 0);
	lv_obj_add_flag(m_confirmMboxOverlay, LV_OBJ_FLAG_CLICKABLE); // Asegurar que capture clics

	m_confirmMbox = lv_msgbox_create(m_modal);
	lv_msgbox_add_title(m_confirmMbox, "Confirmación");
	lv_msgbox_add_text(m_confirmMbox, "¿Estás seguro que deseas cerrar?");

	lv_obj_t * btn;
	btn = lv_msgbox_add_footer_button(m_confirmMbox, "Sí");
	lv_obj_add_event_cb(btn, onConfirmYesPressedCb, LV_EVENT_CLICKED, this);

	btn = lv_msgbox_add_footer_button(m_confirmMbox, "No");
	lv_obj_add_event_cb(btn, onConfirmNoPressedCb, LV_EVENT_CLICKED, this);

	// Centrar en pantalla
	lv_obj_center(m_confirmMbox);
	lv_obj_center(m_confirmMboxOverlay);
}

void PaymentModal::onConfirmYesPressedCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));
	self->hide();
}

void PaymentModal::onConfirmNoPressedCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));

	if (self->m_confirmMbox) {
		lv_msgbox_close(self->m_confirmMbox);
		self->m_confirmMbox = nullptr;
	}
	if (self->m_confirmMboxOverlay) {
		lv_obj_delete(self->m_confirmMboxOverlay);
		self->m_confirmMboxOverlay = nullptr;
	}
}

void PaymentModal::appendPinChar(char c) {
	if (m_pinLen < PIN_MAX_LEN) {
		m_pinBuffer[m_pinLen++] = c;
		m_pinBuffer[m_pinLen] = '\0';
		updatePinDisplay();
		lv_label_set_text(m_lblError, "");
	}
}

void PaymentModal::deletePinChar() {
	if (m_pinLen > 0) {
		m_pinLen--;
		m_pinBuffer[m_pinLen] = '\0';
		updatePinDisplay();
	}
}

void PaymentModal::clearPin() {
	m_pinLen = 0;
	m_pinBuffer[0] = '\0';
	updatePinDisplay();
}

void PaymentModal::updatePinDisplay() {
	if (!m_lblPinField) return;

	if (m_pinLen == 0) {
		lv_label_set_text(m_lblPinField, "");
		return;
	}

	char formattedStr[32] = ""; // Suficiente espacio para 9 dígitos + 2 espacios + null
	int writeIdx = 0;

	for (int i = 0; i < m_pinLen; i++) {
		// ¿Cuántos dígitos quedan por escribir (incluyendo el actual)?
		int remaining = m_pinLen - i;

		// Si es un múltiplo de 3 y no es el primer carácter que escribimos, añadir espacio
		// Pero la regla es "cada 3 desde la derecha". 
		// Ej 7 dígitos: [D] [DDD] [DDD] -> Espacio si remaining % 3 == 0 y NO es el inicio.
		if (remaining % 3 == 0 && i > 0) {
			formattedStr[writeIdx++] = ' ';
		}
		formattedStr[writeIdx++] = m_pinBuffer[i];
	}
	formattedStr[writeIdx] = '\0';

	lv_label_set_text(m_lblPinField, formattedStr);
}

void PaymentModal::showError(const char* msg) {
	if (m_lblError) {
		lv_label_set_text(m_lblError, msg);
	}
}

void PaymentModal::showWrongPinMessage() {
	if (m_wrongPinMbox) return;

	// Overlay para bloquear fondo
	m_wrongPinMboxOverlay = lv_obj_create(m_modal);
	lv_obj_set_size(m_wrongPinMboxOverlay, 600, 420);
	lv_obj_set_style_bg_color(m_wrongPinMboxOverlay, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(m_wrongPinMboxOverlay, LV_OPA_70, 0);
	lv_obj_set_style_border_width(m_wrongPinMboxOverlay, 0, 0);
	lv_obj_add_flag(m_wrongPinMboxOverlay, LV_OBJ_FLAG_CLICKABLE);

	m_wrongPinMbox = lv_msgbox_create(m_modal);
	lv_msgbox_add_title(m_wrongPinMbox, "PIN incorrecto");
	
	// Estilizar la barra de título (header) en rojo
	lv_obj_t* header = lv_msgbox_get_header(m_wrongPinMbox);
	if (header) {
		lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_RED), 0);
		lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
		lv_obj_set_style_text_color(header, lv_color_white(), 0);
	}

	lv_msgbox_add_text(m_wrongPinMbox, "El PIN ingresado no es válido. Por favor, reintente.");

	lv_obj_t* btn = lv_msgbox_add_footer_button(m_wrongPinMbox, "Aceptar");
	lv_obj_add_event_cb(btn, onWrongPinOkCb, LV_EVENT_CLICKED, this);

	lv_obj_center(m_wrongPinMboxOverlay);
	lv_obj_center(m_wrongPinMbox);
}

void PaymentModal::onWrongPinOkCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));
	
	if (self->m_wrongPinMbox) {
		lv_msgbox_close(self->m_wrongPinMbox);
		self->m_wrongPinMbox = nullptr;
	}
	if (self->m_wrongPinMboxOverlay) {
		lv_obj_delete(self->m_wrongPinMboxOverlay);
		self->m_wrongPinMboxOverlay = nullptr;
	}

	self->clearPin();
}
