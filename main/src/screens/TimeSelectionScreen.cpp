#include "screens/TimeSelectionScreen.hpp"
#include <stdio.h> // para snprintf

TimeSelectionScreen::TimeSelectionScreen(std::function<void(ChargePoint*)> onConfirm)
	: m_lblTerminal(nullptr)
	, m_arc(nullptr)
	, m_lblTimeMain(nullptr)
	, m_lblTimeSub(nullptr)
	, m_btnMinus(nullptr)
	, m_btnPlus(nullptr)
	, m_btnConfirm(nullptr)
	, m_arrowLeft(nullptr)
	, m_arrowRight(nullptr)
	, m_activePoint(nullptr)
	, m_onConfirm(onConfirm)
{}

void TimeSelectionScreen::build(lv_obj_t* scr) {
	// --- Título del terminal ---
	m_lblTerminal = lv_label_create(scr);
	lv_label_set_text(m_lblTerminal, "SELECCIONE TERMINAL");
	lv_obj_set_style_text_font(m_lblTerminal, &lv_font_montserrat_18, 0);
	lv_obj_align(m_lblTerminal, LV_ALIGN_TOP_MID, 0, 120);

	// --- Arco Central ---
	m_arc = lv_arc_create(scr);
	lv_obj_set_size(m_arc, 350, 350);
	lv_arc_set_rotation(m_arc, 180);
	lv_arc_set_bg_angles(m_arc, 0, 180);
	// Rango de arcos de 0 a 180 grados, no porcentajes.
	lv_arc_set_range(m_arc, 0, 180);
	lv_arc_set_value(m_arc, 0); // Valor inicial
	lv_obj_set_style_arc_width(m_arc, 8, LV_PART_MAIN);
	lv_obj_set_style_arc_width(m_arc, 8, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(m_arc, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_arc_color(m_arc, lv_color_black(), LV_PART_INDICATOR);
	lv_obj_clear_flag(m_arc, LV_OBJ_FLAG_CLICKABLE); // Solo visual, no touch
	lv_obj_align(m_arc, LV_ALIGN_CENTER, 0, 100);

	// --- Textos de Tiempo ---
	m_lblTimeMain = lv_label_create(scr);
	lv_label_set_text(m_lblTimeMain, "--:--");
	lv_obj_set_style_text_font(m_lblTimeMain, &lv_font_montserrat_48, 0);
	lv_obj_align(m_lblTimeMain, LV_ALIGN_CENTER, 0, 30);

	m_lblTimeSub = lv_label_create(scr);
	lv_label_set_text(m_lblTimeSub, "-- minutos");
	lv_obj_align(m_lblTimeSub, LV_ALIGN_CENTER, 0, 80);

	// --- Botón [-] ---
	m_btnMinus = lv_button_create(scr);
	lv_obj_set_size(m_btnMinus, 60, 60);
	lv_obj_align(m_btnMinus, LV_ALIGN_CENTER, -100, 80);
	lv_obj_set_style_bg_color(m_btnMinus, lv_color_hex(0xBDC3C7), 0);
	lv_obj_t* lblMin = lv_label_create(m_btnMinus);
	lv_label_set_text(lblMin, "-");
	lv_obj_center(lblMin);
	lv_obj_add_event_cb(m_btnMinus, onMinusPressedCb, LV_EVENT_CLICKED, this);

	// --- Botón [+] ---
	m_btnPlus = lv_button_create(scr);
	lv_obj_set_size(m_btnPlus, 60, 60);
	lv_obj_align(m_btnPlus, LV_ALIGN_CENTER, 100, 80);
	lv_obj_set_style_bg_color(m_btnPlus, lv_color_hex(0xBDC3C7), 0);
	lv_obj_t* lblPlus = lv_label_create(m_btnPlus);
	lv_label_set_text(lblPlus, "+");
	lv_obj_center(lblPlus);
	lv_obj_add_event_cb(m_btnPlus, onPlusPressedCb, LV_EVENT_CLICKED, this);

	// --- Botón [USAR] ---
	m_btnConfirm = lv_button_create(scr);
	lv_obj_set_size(m_btnConfirm, 180, 60);
	lv_obj_align(m_btnConfirm, LV_ALIGN_BOTTOM_MID, 0, -50);
	lv_obj_set_style_bg_color(m_btnConfirm, lv_color_hex(0xA3D9A5), 0);
	lv_obj_set_style_border_width(m_btnConfirm, 2, 0);
	lv_obj_set_style_border_color(m_btnConfirm, lv_color_black(), 0);
	lv_obj_t* lblUse = lv_label_create(m_btnConfirm);
	lv_label_set_text(lblUse, "USAR");
	lv_obj_set_style_text_color(lblUse, lv_color_black(), 0);
	lv_obj_set_style_text_font(lblUse, &lv_font_montserrat_22, 0);
	lv_obj_center(lblUse);
	lv_obj_add_event_cb(m_btnConfirm, onConfirmPressedCb, LV_EVENT_CLICKED, this);

	// --- Flechas laterales ---
	m_arrowLeft = lv_label_create(scr);
	lv_label_set_text(m_arrowLeft, LV_SYMBOL_LEFT);
	lv_obj_set_style_text_font(m_arrowLeft, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(m_arrowLeft, lv_palette_main(LV_PALETTE_GREEN), 0);
	lv_obj_align(m_arrowLeft, LV_ALIGN_LEFT_MID, 40, 50);

	m_arrowRight = lv_label_create(scr);
	lv_label_set_text(m_arrowRight, LV_SYMBOL_RIGHT);
	lv_obj_set_style_text_font(m_arrowRight, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(m_arrowRight, lv_palette_main(LV_PALETTE_GREEN), 0);
	lv_obj_align(m_arrowRight, LV_ALIGN_RIGHT_MID, -40, 50);

	// Estado inicial: todo deshabilitado hasta que se asigne un punto
	setActivePoint(nullptr);
}

void TimeSelectionScreen::setActivePoint(ChargePoint* cp) {
	m_activePoint = cp;
	updateDisplay();
}

void TimeSelectionScreen::updateDisplay() {
	if (!m_activePoint) {
		lv_label_set_text(m_lblTerminal, "SELECCIONE TERMINAL");
		lv_label_set_text(m_lblTimeMain, "--:--");
		lv_label_set_text(m_lblTimeSub, "-- minutos");
		lv_arc_set_value(m_arc, 0);
		lv_obj_add_state(m_btnMinus, LV_STATE_DISABLED);
		lv_obj_add_state(m_btnPlus, LV_STATE_DISABLED);
		lv_obj_add_state(m_btnConfirm, LV_STATE_DISABLED);
		return;
	}

	lv_label_set_text_fmt(m_lblTerminal, "TERMINAL %02d", m_activePoint->getId());

	uint16_t mins = m_activePoint->getSelectedMinutes();

	// Etiqueta h:mm
	char buf[16];
	formatTime(mins, buf, sizeof(buf));
	lv_label_set_text(m_lblTimeMain, buf);

	// Etiqueta minutos totales
	lv_label_set_text_fmt(m_lblTimeSub, "%d minutos", mins);

	// Arco (0 grados = 15m, 180 grados = 120m)
	lv_arc_set_value(m_arc, minutesToArcAngle(mins));

	// Estados de botones
	lv_obj_clear_state(m_btnMinus, LV_STATE_DISABLED);
	lv_obj_clear_state(m_btnPlus, LV_STATE_DISABLED);
	lv_obj_clear_state(m_btnConfirm, LV_STATE_DISABLED);
}

uint16_t TimeSelectionScreen::minutesToArcAngle(uint16_t minutes) const {
	const uint16_t MIN_TIME = ChargePoint::getMinTime();
	const uint16_t MAX_TIME = ChargePoint::getMaxTime();
	if (minutes <= MIN_TIME) return 0;
	if (minutes >= MAX_TIME) return 180;
	// Interpolar de [min, max] a [0, 180]
	return ((minutes - MIN_TIME) * 180) / (MAX_TIME - MIN_TIME);
}

void TimeSelectionScreen::formatTime(uint16_t minutes, char* buf, size_t bufSize) const {
	uint16_t h = minutes / 60;
	uint16_t m = minutes % 60;
	snprintf(buf, bufSize, "%d:%02d", h, m);
}

void TimeSelectionScreen::onMinusPressedCb(lv_event_t* e) {
	auto* self = static_cast<TimeSelectionScreen*>(lv_event_get_user_data(e));
	if (self && self->m_activePoint) {
		self->m_activePoint->decrementTime();
		self->updateDisplay();
	}
}

void TimeSelectionScreen::onPlusPressedCb(lv_event_t* e) {
	auto* self = static_cast<TimeSelectionScreen*>(lv_event_get_user_data(e));
	if (self && self->m_activePoint) {
		self->m_activePoint->incrementTime();
		self->updateDisplay();
	}
}

void TimeSelectionScreen::onConfirmPressedCb(lv_event_t* e) {
	auto* self = static_cast<TimeSelectionScreen*>(lv_event_get_user_data(e));
	if (self && self->m_onConfirm && self->m_activePoint) {
		self->m_onConfirm(self->m_activePoint);
	}
}
