#pragma once
#include <stdint.h>
#include <string.h>  // memset, memcpy

/**
 * @brief Estado de un punto de carga.
 */
enum class ChargePointStatus : uint8_t {
	AVAILABLE,      ///< Libre y listo para usar
	OCCUPIED,       ///< En uso
	FAULT,          ///< Error de hardware
	OUT_OF_SERVICE  ///< Fuera de servicio / deshabilitado
};

enum class ChargeWorkMode : uint16_t {
	IDLE,
	PROCESSING,
	DONE,
	ERROR
};

/**
 * @brief Modelo de dominio de un punto de carga.
 *
 * Encapsula el estado y la lógica de selección de tiempo de un
 * punto de carga individual. No tiene dependencias de LVGL ni ESP-IDF.
 *
 * Reglas de tiempo:
 *  - Paso fijo: 15 minutos
 *  - Mínimo    : 15 minutos
 *  - Máximo    : 120 minutos (2 horas)
 */
class ChargePoint {
public:
	explicit ChargePoint(uint8_t id);

	// --- Identidad y estado ---
	uint8_t           getId()     const;
	ChargePointStatus getStatus() const;
	void              setStatus(ChargePointStatus s);
	bool              isAvailable() const;

	// --- Selección de tiempo ---
	/**
	 * @brief Incrementa el tiempo seleccionado en 15 minutos.
	 * @return true si el incremento fue posible, false si ya estaba al máximo.
	 */
	bool incrementTime();

	/**
	 * @brief Decrementa el tiempo seleccionado en 15 minutos.
	 * @return true si el decremento fue posible, false si ya estaba al mínimo.
	 */
	bool decrementTime();

	uint16_t getSelectedMinutes() const;

	/**
	 * @brief Resetea el tiempo seleccionado al valor mínimo (15 min).
	 */
	void resetTime();

	// --- Datos de sesión (rellenados por ControlBoardProxy) ---

	/** Firma de 64 bytes generada por el esclavo para el QR. */
	const uint8_t* getSignature() const { return m_signature; }
	void setSignature(const uint8_t* sig) { memcpy(m_signature, sig, 64); }

	/**
	 * Precio de la sesión con 3 decimales implícitos.
	 * Ej: 12345 representa 12.345
	 */
	uint32_t getPrice() const { return m_price; }
	void     setPrice(uint32_t p) { m_price = p; }

	/** Watts consumidos en la sesión actual. */
	uint16_t getEnergy() const { return m_energy; }
	void     setEnergy(uint16_t w) { m_energy = w; }

	/** Minutos transcurridos suministrando energía. */
	uint16_t getElapsedTime() const { return m_elapsedTime; }
	void     setElapsedTime(uint16_t t) { m_elapsedTime = t; }

private:
	static constexpr uint16_t TIME_STEP_MIN = 15;
	static constexpr uint16_t TIME_MIN_MIN  = 15;
	static constexpr uint16_t TIME_MAX_MIN  = 120;

	// Identidad y estado local
	uint8_t           m_id;
	ChargePointStatus m_status;
	uint16_t          m_selectedMinutes;

	// Datos de sesión provenientes del esclavo Modbus
	uint8_t  m_signature[64];
	uint32_t m_price;
	uint16_t m_energy;
	uint16_t m_elapsedTime;
};
