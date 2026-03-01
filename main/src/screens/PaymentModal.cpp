#include "screens/PaymentModal.hpp"
#include "../components/crypto_payment/include/CryptoPayment.h"
#include <stdio.h>

PaymentModal::PaymentModal(CryptoPayment* crypto,
						   std::function<void(ChargePoint*)> onValidated)
	: m_modal(nullptr)
	, m_lblTitle(nullptr)
	, m_lblPrice(nullptr)
	, m_qrCanvas(nullptr)
	, m_lblPinField(nullptr)
	, m_lblError(nullptr)
	, m_btnValidate(nullptr)
	, m_pinLen(0)
	, m_activePoint(nullptr)
	, m_crypto(crypto)
	, m_onValidated(onValidated)
{
	for (int i = 0; i < 12; i++) {
		m_numpadBtns[i] = nullptr;
	}
	m_pinBuffer[0] = '\0';
}

void PaymentModal::show(lv_obj_t* scr, ChargePoint* cp) {
	if (!cp || !m_crypto) return;
	m_activePoint = cp;

	// Fondo oscurecido
	m_modal = lv_obj_create(scr);
	lv_obj_set_size(m_modal, 800, 480);
	lv_obj_center(m_modal);
	lv_obj_set_style_bg_color(m_modal, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(m_modal, LV_OPA_70, 0);
	lv_obj_clear_flag(m_modal, LV_OBJ_FLAG_SCROLLABLE);

	// Contenedor blanco central
	lv_obj_t* box = lv_obj_create(m_modal);
	lv_obj_set_size(box, 560, 380);
	lv_obj_center(box);
	lv_obj_set_style_bg_color(box, lv_color_white(), 0);
	lv_obj_set_style_radius(box, 15, 0);
	lv_obj_set_style_border_color(box, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
	lv_obj_set_style_border_width(box, 4, 0);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

	// Título
	m_lblTitle = lv_label_create(box);
	lv_label_set_text_fmt(m_lblTitle, "START CHARGING - TERMINAL %02d", cp->getId());
	lv_obj_set_style_text_font(m_lblTitle, &lv_font_montserrat_22, 0);
	lv_obj_align(m_lblTitle, LV_ALIGN_TOP_MID, 0, 10);

	// Calcular precio (ejemplo: 0.50 por cada 15 min)
	float price = (cp->getSelectedMinutes() / 15.0f) * 0.50f;
	m_lblPrice = lv_label_create(box);
	lv_label_set_text_fmt(m_lblPrice, "S/ %.2f", price);
	lv_obj_set_style_text_font(m_lblPrice, &lv_font_montserrat_24, 0);
	lv_obj_align(m_lblPrice, LV_ALIGN_TOP_LEFT, 40, 60);

	// Generar Payload y QR
	uint8_t payload[256];
	size_t payloadLen = 0;
	if (m_crypto->generatePayload(cp->getId(), cp->getSelectedMinutes(), payload, sizeof(payload), payloadLen)) {
		m_qrCanvas = QRRenderer::render(box, payload, payloadLen, 40, 100, 4);
	}

	// Label bajo el QR
	lv_obj_t* lblScan = lv_label_create(box);
	lv_label_set_text(lblScan, "SCAN TO PAY");
	lv_obj_set_style_text_font(lblScan, &lv_font_montserrat_16, 0);
	lv_obj_align(lblScan, LV_ALIGN_TOP_LEFT, 60, 270);

	// Etiqueta ENTER PIN y campo de PIN
	lv_obj_t* lblEnterPin = lv_label_create(box);
	lv_label_set_text(lblEnterPin, "ENTER PIN");
	lv_obj_set_style_text_font(lblEnterPin, &lv_font_montserrat_14, 0);
	lv_obj_align(lblEnterPin, LV_ALIGN_TOP_RIGHT, -140, 65);

	m_lblPinField = lv_label_create(box);
	lv_label_set_text(m_lblPinField, "");
	lv_obj_set_size(m_lblPinField, 120, 30);
	lv_obj_set_style_bg_color(m_lblPinField, lv_color_hex(0xE0F7FA), 0);
	lv_obj_set_style_bg_opa(m_lblPinField, LV_OPA_COVER, 0);
	lv_obj_set_style_text_align(m_lblPinField, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(m_lblPinField, &lv_font_montserrat_20, 0);
	lv_obj_set_style_radius(m_lblPinField, 5, 0);
	lv_obj_align(m_lblPinField, LV_ALIGN_TOP_RIGHT, -20, 60);

	// Teclado numérico
	buildNumpad();
	lv_obj_align(lv_obj_get_parent(m_numpadBtns[0]), LV_ALIGN_TOP_RIGHT, -20, 100);

	// Error label (oculto por defecto)
	m_lblError = lv_label_create(box);
	lv_label_set_text(m_lblError, "");
	lv_obj_set_style_text_color(m_lblError, lv_palette_main(LV_PALETTE_RED), 0);
	lv_obj_align(m_lblError, LV_ALIGN_BOTTOM_MID, 0, -60);

	// Botón Validate
	m_btnValidate = lv_button_create(box);
	lv_obj_set_size(m_btnValidate, 200, 50);
	lv_obj_align(m_btnValidate, LV_ALIGN_BOTTOM_MID, 0, -10);
	lv_obj_set_style_bg_color(m_btnValidate, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
	lv_obj_set_style_radius(m_btnValidate, 8, 0);

	lv_obj_t* lblVal = lv_label_create(m_btnValidate);
	lv_label_set_text(lblVal, "VALIDATE PIN");
	lv_obj_set_style_text_font(lblVal, &lv_font_montserrat_18, 0);
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
	clearPin();
}

void PaymentModal::buildNumpad() {
	// The parent is the box, we will create a grid or simple flex container
	lv_obj_t* numpadCont = lv_obj_create(lv_obj_get_child(m_modal, 0));
	lv_obj_set_size(numpadCont, 220, 160);
	lv_obj_set_style_bg_opa(numpadCont, 0, 0);
	lv_obj_set_style_border_width(numpadCont, 0, 0);
	lv_obj_set_style_pad_all(numpadCont, 0, 0);
	lv_obj_clear_flag(numpadCont, LV_OBJ_FLAG_SCROLLABLE);

	const char* keys[12] = {
		"1", "2", "3",
		"4", "5", "6",
		"7", "8", "9",
		"DEL", "0", "OK"
	};

	int btnWidth = 60;
	int btnHeight = 35;
	int pad = 10;

	for (int i = 0; i < 12; i++) {
		int row = i / 3;
		int col = i % 3;

		m_numpadBtns[i] = lv_button_create(numpadCont);
		lv_obj_set_size(m_numpadBtns[i], btnWidth, btnHeight);
		lv_obj_set_pos(m_numpadBtns[i], col * (btnWidth + pad), row * (btnHeight + pad));
		lv_obj_set_style_bg_color(m_numpadBtns[i], lv_color_white(), 0);
		lv_obj_set_style_border_color(m_numpadBtns[i], lv_color_black(), 0);
		lv_obj_set_style_border_width(m_numpadBtns[i], 2, 0);
		lv_obj_set_style_radius(m_numpadBtns[i], 10, 0);

		lv_obj_t* lbl = lv_label_create(m_numpadBtns[i]);
		lv_label_set_text(lbl, keys[i]);
		lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
		lv_obj_center(lbl);

		lv_obj_add_event_cb(m_numpadBtns[i], onKeyPressedCb, LV_EVENT_CLICKED, this);
		// Store the char or action in user_data
		lv_obj_set_user_data(m_numpadBtns[i], (void*)(uintptr_t)keys[i][0]); // solo el primer char ('1', 'D', 'O')
	}
}

void PaymentModal::onKeyPressedCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));
	lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
	char action = (char)(uintptr_t)lv_obj_get_user_data(btn);

	if (action >= '0' && action <= '9') {
		self->appendPinChar(action);
	} else if (action == 'D') { // DEL
		self->deletePinChar();
	} else if (action == 'O') { // OK
		// Tratar como si presionaran VALIDATE PIN
		//lv_event_send(self->m_btnValidate, LV_EVENT_CLICKED, nullptr);
	}
}

void PaymentModal::onValidatePressedCb(lv_event_t* e) {
	auto* self = static_cast<PaymentModal*>(lv_event_get_user_data(e));
	if (!self->m_activePoint || self->m_pinLen == 0) return;

	uint32_t pin = 0;
	sscanf(self->m_pinBuffer, "%lu", &pin);

	bool valid = self->m_crypto->validatePIN(pin, self->m_activePoint->getId(), self->m_activePoint->getSelectedMinutes());

	if (valid) {
		if (self->m_onValidated) {
			self->m_onValidated(self->m_activePoint);
		}
	} else {
		self->showError("INVALID PIN! TRY AGAIN.");
		self->clearPin();
	}
}

void PaymentModal::appendPinChar(char c) {
	if (m_pinLen < PIN_MAX_LEN) {
		m_pinBuffer[m_pinLen++] = c;
		m_pinBuffer[m_pinLen] = '\0';
		updatePinDisplay();
		lv_label_set_text(m_lblError, ""); // Limpia error si empieza a escribir de nuevo
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
	char displayStr[16] = "";
	for (int i = 0; i < m_pinLen; i++) {
		displayStr[i] = '*';
	}
	displayStr[m_pinLen] = '\0';
	lv_label_set_text(m_lblPinField, displayStr);
}

void PaymentModal::showError(const char* msg) {
	if (m_lblError) {
		lv_label_set_text(m_lblError, msg);
	}
}
