#pragma once

#include <cstdint>
#include <hal/gpio_types.h>

/// WiFi AP to connect to.
static constexpr const char * const WIFI_SSID = "";

/// Password to use for @ref WIFI_SSID.
static constexpr const char * const WIFI_PASSWORD = "";

/// Hostname to assign to the device upon connection to WiFi.
static constexpr const char * const WIFI_HOSTNAME = "esp32feeder";

/// Number of banks of feeders to configure, each bank can hold up to 32 feeders.
static constexpr std::size_t FEEDER_BANK_COUNT = 2;

/// Pin to use for I2C SCL.
static constexpr gpio_num_t I2C_SCL_PIN_NUM = GPIO_NUM_22;

/// Pin to use for I2C SDA.
static constexpr gpio_num_t I2C_SDA_PIN_NUM = GPIO_NUM_23;

/// I2C Bus Speed in Hz.
static constexpr uint32_t I2C_BUS_SPEED = 100000;