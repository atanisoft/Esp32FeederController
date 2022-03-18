#pragma once
#include <stdint.h>

#include "sdkconfig.h"

#include <esp_idf_version.h>
#if defined(CONFIG_IDF_TARGET_ESP32)
#include <esp32/rom/rtc.h>
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#include <esp32s2/rom/rtc.h>
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include <esp32s3/rom/rtc.h>
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#include <esp32c3/rom/rtc.h>
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
#include <esp32h2/rom/rtc.h>
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
#include <esp32c2/rom/rtc.h>
#endif

/// Utility class which logs information about the currently running SoC.
class SocInfo
{
public:
    /// Logs information about the currently running SoC.
    ///
    /// @return Reason for the reset of the SoC.
    static uint8_t print_soc_info();
};
