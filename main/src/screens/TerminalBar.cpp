#include "screens/TerminalBar.hpp"

TerminalBar::TerminalBar(std::vector<ChargePoint>& chargePoints,
						 SelectCallback onSelect)
	: m_chargePoints(chargePoints)
	, m_onSelect(onSelect)
	, m_container(nullptr)
	, m_activeId(0)
{}

void TerminalBar::build(lv_obj_t* scr) {
	if (m_chargePoints.empty()) return;

	// Contenedor superior (fila flexible)
	m_container = lv_obj_create(scr);
	lv_obj_set_size(m_container, 800, 100);
	lv_obj_align(m_container, LV_ALIGN_TOP_MID, 0, 10);
	lv_obj_clear_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_opa(m_container, 0, 0);
	lv_obj_set_style_border_width(m_container, 0, 0);
	lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(m_container, 20, 0); // Espacio entre íconos

	// Crear un ícono por cada ChargePoint
	m_icons.reserve(m_chargePoints.size());
	for (auto& cp : m_chargePoints) {
		lv_obj_t* btn = lv_button_create(m_container);
		lv_obj_set_size(btn, 80, 80);
		lv_obj_set_style_bg_opa(btn, 0, 0); // Fondo transparente
		lv_obj_set_style_shadow_width(btn, 0, 0);

		// Guardar el puntero a TerminalBar y el ID en el user_data del evento
		lv_obj_add_event_cb(btn, onIconClickedCb, LV_EVENT_CLICKED, this);
		// Truco: guardaremos el ID directamente en el ID del objeto para recuperarlo fácil
		lv_obj_set_user_data(btn, reinterpret_cast<void*>(cp.getId()));

		// Símbolo del rayo
		lv_obj_t* icon = lv_label_create(btn);
		lv_label_set_text(icon, LV_SYMBOL_CHARGE);
		lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
		lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

		// Número debajo
		lv_obj_t* lbl = lv_label_create(btn);
		lv_label_set_text_fmt(lbl, "%02d", cp.getId());
		lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
		lv_obj_set_style_text_color(lbl, lv_color_black(), 0);

		m_icons.push_back(btn);
		refreshIcon(cp.getId()); // Pintarlo del color correcto según su estado inicial
	}
}

void TerminalBar::setActive(uint8_t id) {
	if (m_activeId == id) return;

	// Quitar borde/marca al anterior
	if (m_activeId > 0 && m_activeId <= m_icons.size()) {
		lv_obj_t* oldBtn = m_icons[m_activeId - 1];
		lv_obj_set_style_border_width(oldBtn, 0, 0);
	}

	m_activeId = id;

	// Poner borde/marca al actual
	if (m_activeId > 0 && m_activeId <= m_icons.size()) {
		lv_obj_t* newBtn = m_icons[m_activeId - 1];
		lv_obj_set_style_border_width(newBtn, 2, 0);
		lv_obj_set_style_border_color(newBtn, lv_color_black(), 0);
		lv_obj_set_style_radius(newBtn, 10, 0);
	}
}

void TerminalBar::refreshIcon(uint8_t id) {
	if (id == 0 || id > m_icons.size() || id > m_chargePoints.size()) return;

	lv_obj_t* btn = m_icons[id - 1];
	lv_obj_t* icon = lv_obj_get_child(btn, 0); // El primer hijo es el rayo
	ChargePointStatus status = m_chargePoints[id - 1].getStatus();

	lv_color_t color;
	switch (status) {
		case ChargePointStatus::AVAILABLE:      color = lv_palette_main(LV_PALETTE_BLUE); break;
		case ChargePointStatus::OCCUPIED:       color = lv_palette_main(LV_PALETTE_YELLOW); break;
		case ChargePointStatus::FAULT:          color = lv_palette_main(LV_PALETTE_RED); break;
		case ChargePointStatus::OUT_OF_SERVICE: color = lv_palette_main(LV_PALETTE_GREY); break;
		default:                                color = lv_color_black(); break;
	}

	lv_obj_set_style_text_color(icon, color, 0);
}

void TerminalBar::onIconClickedCb(lv_event_t* e) {
	auto* self = static_cast<TerminalBar*>(lv_event_get_user_data(e));
	lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);

	// Recuperar el ID guardado en user_data
	uint8_t id = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target));

	if (self && self->m_onSelect) {
		self->m_onSelect(id);
	}
}
