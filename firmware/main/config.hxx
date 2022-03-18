#pragma once

#include <stdint.h>

/// WiFi AP to connect to.
static constexpr const char * const WIFI_SSID = "";

/// Password to use for @ref WIFI_SSID.
static constexpr const char * const WIFI_PASSWORD = "";

/// Hostname to assign to the device upon connection to WiFi.
static constexpr const char * const WIFI_HOSTNAME = "esp32feeder";

/// Number of banks of feeders to configure, each bank can hold up to 48 feeders.
static constexpr size_t FEEDER_BANK_COUNT = 2;

/// Maximum number of feeders to configure per bank.
static constexpr size_t MAX_FEEDERS_PER_BANK = 48;

/// Total number of feeders to configure.
static constexpr size_t TOTAL_FEEDER_COUNT = MAX_FEEDERS_PER_BANK * FEEDER_BANK_COUNT;
