set(IDF_DEPS
    app_update
    asio
    driver
    esp_system
    heap
    nvs_flash
    pthread
    vfs)

idf_component_register(
    SRCS main.cpp WiFiManager.cpp SocInfo.cpp FeederManager.cpp Feeder.cpp GCodeServer.cpp I2Cbus.cpp
    REQUIRES "${IDF_DEPS}"
)
