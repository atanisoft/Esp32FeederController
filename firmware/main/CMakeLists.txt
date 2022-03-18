set(IDF_DEPS
    asio
    driver
    esp_system
    heap
    nvs_flash
    vfs)

idf_component_register(
    SRCS main.cpp WiFiManager.cpp
    REQUIRES "${IDF_DEPS}"
)
