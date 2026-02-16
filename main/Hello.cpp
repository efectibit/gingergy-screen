#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_chip_info.h"
#include "esp_log.h"

static const char *TAG = "example";

extern "C" void app_main(void) {
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);

	ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), %s%s%s%s, ",
		CONFIG_IDF_TARGET,
		chip_info.cores,
		(chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
		(chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
		(chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
		(chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

	for (int i = 10; i >= 0; i--) {
		ESP_LOGI(TAG, "Restarting in %d seconds...", i);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	ESP_LOGI(TAG, "Restarting now.");
}
