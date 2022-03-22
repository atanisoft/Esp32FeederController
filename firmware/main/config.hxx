#pragma once

#include <cstdint>
#include <hal/gpio_types.h>

/// WiFi AP to connect to.
static constexpr const char * const WIFI_SSID = "";

/// Password to use for @ref WIFI_SSID.
static constexpr const char * const WIFI_PASSWORD = "";

/// Hostname to assign to the device upon connection to WiFi.
static constexpr const char * const WIFI_HOSTNAME = "esp32feeder";

/// Base address to start from when searching for PCA9685 devices, typically
/// this should not need to be changed.
static constexpr uint8_t PCA9685_BASE_ADDRESS = 0x40;

/// Default frequency to use for sending pulses to the connected servos, the
/// value below generates an approximate 20ms pulse.
static constexpr uint32_t PCA9685_FREQUENCY = 50;

/// Maximum number of PCA9685 devices to search for.
static constexpr std::size_t MAX_PCA9685_COUNT = 10;

/// Default fully extended angle in degrees.
static constexpr uint8_t DEFAULT_FEEDER_FULL_ADVANCE_ANGLE = 90;

/// Default fully extended angle in degrees.
static constexpr uint8_t DEFAULT_FEEDER_RETRACT_ANGLE = 15;

/// Default settlement time in milliseconds.
static constexpr uint16_t DEFAULT_FEEDER_SETTLE_TIME_MS = 240;

/// Default minimum number of pulses to send the servo.
static constexpr uint16_t DEFAULT_FEEDER_MIN_PULSE_COUNT = 150;

/// Default maximum number of pulses to send the servo.
static constexpr uint16_t DEFAULT_FEEDER_MAX_PULSE_COUNT = 600;

/// Pin to use for I2C SCL.
static constexpr gpio_num_t I2C_SCL_PIN_NUM = GPIO_NUM_22;

/// Pin to use for I2C SDA.
static constexpr gpio_num_t I2C_SDA_PIN_NUM = GPIO_NUM_23;

/// I2C Bus Speed in Hz.
static constexpr uint32_t I2C_BUS_SPEED = 100000;

/// NVS namespace to use for all configuration data.
static constexpr const char *const NVS_FEEDER_NAMESPACE = "esp32feeder";
