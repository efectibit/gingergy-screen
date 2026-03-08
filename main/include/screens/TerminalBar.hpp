#pragma once
#include <vector>
#include <functional>
#include "lvgl.h"
#include "../ChargePoint.hpp"

/**
 * @brief Barra superior de íconos de puntos de carga.
 *
 * Muestra un ícono de rayo por cada ChargePoint configurado.
 * El color del ícono refleja el estado del punto:
 *   - AVAILABLE     → Azul
 *   - OCCUPIED      → Amarillo
 *   - FAULT         → Rojo
 *   - OUT_OF_SERVICE→ Gris
 *
 * Al tocar un ícono se invoca el callback onSelect con el id del punto.
 */
class TerminalBar {
public:
	using SelectCallback = std::function<void(uint8_t)>;

	/**
	 * @param chargePoints  Referencia al vector de puntos de carga.
	 * @param onSelect      Callback invocado cuando el usuario toca un ícono.
	 *                      Recibe el id del ChargePoint seleccionado.
	 */
	TerminalBar(std::vector<ChargePoint>& chargePoints,
				SelectCallback onSelect);

	/**
	 * @brief Construye los widgets LVGL sobre la pantalla dada.
	 * @param scr  Pantalla LVGL activa.
	 */
	void build(lv_obj_t* scr);

	/**
	 * @brief Marca visualmente el terminal activo y quita la marca del anterior.
	 * @param id  ID del ChargePoint a marcar como activo.
	 */
	void setActive(uint8_t id);

	/**
	 * @brief Actualiza el color de un ícono según el estado actual del punto.
	 * @param id  ID del ChargePoint a refrescar.
	 */
	void refreshIcon(uint8_t id);

private:
	static void onIconClickedCb(lv_event_t* e);

	std::vector<ChargePoint>& m_chargePoints;
	SelectCallback           m_onSelect;

	lv_obj_t*              m_container;
	std::vector<lv_obj_t*> m_icons;   ///< Un ícono por ChargePoint
	uint8_t                m_activeId;
};
