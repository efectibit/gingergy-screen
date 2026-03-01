#include "screens/PaymentModal.hpp"
#include "../components/crypto_payment/include/CryptoPayment.h"

PaymentModal::PaymentModal(CryptoPayment* crypto,
						   std::function<void(ChargePoint*)> onValidated)
	: m_modal(nullptr)
	, m_lblTitle(nullptr)
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

void PaymentModal::show(lv_obj_t* scr, ChargePoint* cp) {}
void PaymentModal::hide() {}

void PaymentModal::buildQR(uint8_t* payload, size_t len) {}
void PaymentModal::buildNumpad() {}
void PaymentModal::buildPinDisplay() {}
void PaymentModal::showError(const char* msg) {}

void PaymentModal::onKeyPressedCb(lv_event_t* e) {}
void PaymentModal::onValidatePressedCb(lv_event_t* e) {}

void PaymentModal::appendPinChar(char c) {}
void PaymentModal::deletePinChar() {}
void PaymentModal::clearPin() {}
void PaymentModal::updatePinDisplay() {}
