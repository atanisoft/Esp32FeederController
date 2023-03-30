set(IDF_DEPS
    app_update
    driver
    esp_hw_support
    esp_netif
    esp_system
    esp_wifi
    heap
    nvs_flash
    pthread
    vfs)

idf_component_register(
    SRCS main.cpp WiFiManager.cpp SocInfo.cpp FeederManager.cpp Feeder.cpp GCodeServer.cpp I2Cbus.cpp
    REQUIRES "${IDF_DEPS}"
)
