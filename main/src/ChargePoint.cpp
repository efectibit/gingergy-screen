#include "ChargePoint.hpp"

// Inicialización de miembros estáticos con valores por defecto
uint16_t ChargePoint::s_timeMin  = 15;
uint16_t ChargePoint::s_timeMax  = 120;
uint16_t ChargePoint::s_timeStep = 15;

ChargePoint::ChargePoint(uint8_t id)
	: m_id(id)
	, m_status(ChargePointStatus::AVAILABLE)
	, m_selectedMinutes(s_timeMin)
	, m_price(0)
	, m_energy(0)
	, m_elapsedTime(0)
	, m_remainingTime(0)
{
	memset(m_signature, 0, sizeof(m_signature));
}

void ChargePoint::setGlobalTimeLimits(uint16_t min, uint16_t max, uint16_t step) {
	if (min > 0) s_timeMin = min;
	if (max > min) s_timeMax = max;
	if (step > 0) s_timeStep = step;
}

uint8_t ChargePoint::getId() const {
	return this->m_id;
}

ChargePointStatus ChargePoint::getStatus() const {
	return this->m_status;
}

void ChargePoint::setStatus(ChargePointStatus s) {
	this->m_status = s;
}

bool ChargePoint::isAvailable() const {
	return this->m_status == ChargePointStatus::AVAILABLE;
}

bool ChargePoint::incrementTime() {
	if (this->m_selectedMinutes >= s_timeMax) {
		return false;
	}
	this->m_selectedMinutes += s_timeStep;
	return true;
}

bool ChargePoint::decrementTime() {
	if (this->m_selectedMinutes <= s_timeMin) {
		return false;
	}
	this->m_selectedMinutes -= s_timeStep;
	return true;
}

uint16_t ChargePoint::getSelectedMinutes() const {
	return this->m_selectedMinutes;
}

void ChargePoint::resetTime() {
	this->m_selectedMinutes = s_timeMin;
}
