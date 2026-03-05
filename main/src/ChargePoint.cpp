#include "ChargePoint.hpp"

ChargePoint::ChargePoint(uint8_t id)
	: m_id(id)
	, m_status(ChargePointStatus::AVAILABLE)
	, m_selectedMinutes(TIME_MIN_MIN)
	, m_price(0)
	, m_energy(0)
	, m_elapsedTime(0)
{
	memset(m_signature, 0, sizeof(m_signature));
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
	if (this->m_selectedMinutes >= TIME_MAX_MIN) {
		return false;
	}
	this->m_selectedMinutes += TIME_STEP_MIN;
	return true;
}

bool ChargePoint::decrementTime() {
	if (this->m_selectedMinutes <= TIME_MIN_MIN) {
		return false;
	}
	this->m_selectedMinutes -= TIME_STEP_MIN;
	return true;
}

uint16_t ChargePoint::getSelectedMinutes() const {
	return this->m_selectedMinutes;
}

void ChargePoint::resetTime() {
	this->m_selectedMinutes = TIME_MIN_MIN;
}
