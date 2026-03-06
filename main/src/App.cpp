#include "App.hpp"
#include "esp_log.h"

static const char* TAG_APP = "App";

// ─── Configuración de hardware (ajustar según instalación) ───────────────────
// UART1 para RS232 Modbus — pines de ejemplo, ajustar según PCB
static constexpr uart_port_t MODBUS_UART   = UART_NUM_1;
static constexpr uint8_t     MODBUS_ADDR   = 0x01;
static constexpr int         MODBUS_TX_PIN = 17;
static constexpr int         MODBUS_RX_PIN = 18;

// =============================================================================
// Constructor
// =============================================================================
App::App(uint8_t numPoints)
	: m_display()
	, m_proxy(MODBUS_UART, MODBUS_ADDR)
	, m_terminalBar(nullptr)
	, m_selectionScreen(nullptr)
	, m_paymentModal(nullptr)
{
	// Crear los ChargePoints (ids 1-based: 1..numPoints)
	m_chargePoints.reserve(numPoints);
	for (uint8_t i = 1; i <= numPoints; i++) {
		m_chargePoints.emplace_back(i);
	}
	ESP_LOGI(TAG_APP, "App creada con %d puntos de carga", numPoints);
}

// =============================================================================
// run() — Punto de entrada principal
// =============================================================================
void App::run() {
	// 1. Inicializar Modbus (UART1: TX=17, RX=18, 115200 bps)
	esp_err_t err = m_proxy.init(MODBUS_TX_PIN, MODBUS_RX_PIN, 115200);
	if (err == ESP_OK) {
		m_proxy.start();
		ESP_LOGI(TAG_APP, "Modbus Master iniciado correctamente");
	} else {
		ESP_LOGE(TAG_APP, "Fallo al inicializar Modbus: %s", esp_err_to_name(err));
	}

	// 2. Inicializar panel físico
	m_display.init();

	// 3. Arrancar LVGL en Core 1; se llama buildUI() cuando LVGL esté listo
	m_display.startGuiTask([this](lv_display_t* disp) {
		this->buildUI(disp);
	});

	// app_main retorna → FreeRTOS sigue corriendo la tarea de LVGL
}

// =============================================================================
// buildUI() — Construye la interfaz gráfica (llamado desde guiTaskFn)
// =============================================================================
void App::buildUI(lv_display_t* disp) {
	lv_obj_t* scr = lv_display_get_screen_active(disp);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(scr, lv_color_hex(0xEEEEEE), 0);

	// 1. Barra superior
	m_terminalBar = new TerminalBar(m_chargePoints, [this](uint8_t id) {
		this->onTerminalSelected(id);
	});
	m_terminalBar->build(scr);

	// 2. Pantalla central (arco y tiempo)
	m_selectionScreen = new TimeSelectionScreen([this](ChargePoint* cp) {
		this->onTimeConfirmed(cp);
	});
	m_selectionScreen->build(scr);

	// 3. Modal de pago (oculto inicialmente)
	m_paymentModal = new PaymentModal([this](ChargePoint* cp, uint32_t pin) {
		this->onPaymentValidate(cp, pin);
	});

	ESP_LOGI(TAG_APP, "UI construida (TerminalBar + TimeSelectionScreen + PaymentModal)");
}

// =============================================================================
// Callbacks de navegación
// =============================================================================
void App::onTerminalSelected(uint8_t id) {
	if (id == 0 || id > m_chargePoints.size()) return;

	ChargePoint* cp = &m_chargePoints[id - 1];

	if (!cp->isAvailable()) {
		ESP_LOGW(TAG_APP, "El terminal %d no está disponible", id);
		return; // Ignorar toques en terminales ocupados/rotos
	}

	ESP_LOGI(TAG_APP, "Terminal %d seleccionado", id);

	m_display.lock();
	m_terminalBar->setActive(id);
	m_selectionScreen->setActivePoint(cp);
	m_display.unlock();
}

void App::onTimeConfirmed(ChargePoint* cp) {
	if (!cp) return;
	ESP_LOGI(TAG_APP, "Tiempo confirmado: terminal %d, %d min",
			 cp->getId(), cp->getSelectedMinutes());

	m_display.lock();
	lv_obj_t* scr = lv_display_get_screen_active(m_display.getDisplay());
	m_paymentModal->show(scr, cp);
	m_display.unlock();

	esp_err_t sigErr = m_proxy.requestPrice(cp);
	if (sigErr != ESP_OK) {
		ESP_LOGW(TAG_APP, "No se pudo obtener firma del esclavo (T%d). El QR usará fallback.",
				 cp->getId());
	}

	// Loop to ask for work mode
	ChargeWorkMode workMode = ChargeWorkMode::UNKNOWN;
	for (int i = 0; i < 4 && workMode != ChargeWorkMode::DONE; i++) {
		workMode = m_proxy.readWorkMode(cp);
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	// Update QR and price
	this->m_display.lock();

	if (workMode == ChargeWorkMode::DONE) {
		this->m_proxy.readPrice(cp);

		const uint8_t* sig = cp->getSignature();
		bool hasSig = false;
		for (int i = 0; i < 64 && !hasSig; i++) {
			hasSig = (sig[i] != 0);
		}

		if (hasSig) {
			this->m_paymentModal->updateQr(sig, 64);
			this->m_paymentModal->updatePrice(cp->getPrice());
		}
	}
	
	this->m_display.unlock();
}

void App::onPaymentValidate(ChargePoint* cp, uint32_t pin) {
	if (!cp) return;
	ESP_LOGI(TAG_APP, "Pago validado: enviando comando Modbus para terminal %d por %d minutos",
			 cp->getId(), cp->getSelectedMinutes());

	// Enviar comando vía Modbus usando el PIN ingresado por el usuario
	esp_err_t err = m_proxy.sendUserPin(cp, pin);
	if (err != ESP_OK) {
		ESP_LOGE(TAG_APP, "Modbus falló al enviar comando al terminal %d", cp->getId());
	}

	m_display.lock();
	m_paymentModal->hide();
	cp->resetTime();
	m_terminalBar->refreshIcon(cp->getId());
	m_selectionScreen->updateDisplay();
	m_display.unlock();
}

void App::syncChargePointStatus() {
	// Por implementar cuando ModbusClient esté completo
}

extern "C" void app_main() {
	// 6 puntos de carga gestionados por la placa controladora
	static App app(6);
	app.run();
}
