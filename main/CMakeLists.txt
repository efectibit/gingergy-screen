file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
)

idf_component_register(
    SRCS
        ${SRC_FILES}
    INCLUDE_DIRS
        "include"
        "../components/crypto_payment/include"
    REQUIRES
        esp_lcd
        driver
        esp_timer
        lvgl
        esp_lcd_touch_gt911
)