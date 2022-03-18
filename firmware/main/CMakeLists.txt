set(IDF_DEPS
    app_update
    asio
    driver
    esp_system
    heap
    nvs_flash
    vfs)

idf_component_register(
    SRCS main.cpp WiFiManager.cpp SocInfo.cpp
    REQUIRES "${IDF_DEPS}"
)
