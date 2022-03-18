
#include <algorithm>
#include <asio.hpp>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <string>
#include "config.hxx"
#include "WiFiManager.hxx"
#include "GCodeServer.hxx"
#include "Utils.hxx"

static constexpr const char *const TAG = "main";
static constexpr const char *const CMD_IMPL_TAG = "command";

static WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD, WIFI_HOSTNAME);

extern "C" void app_main()
{
    configure_log_levels();

    ESP_LOGI(TAG, "Esp32SlottedFeeder v0.0 Initializing");
    ESP_LOGI(TAG, "Banks:%zu, Feeders per bank:%zu, Total Feeders:%zu",
        FEEDER_BANK_COUNT, MAX_FEEDERS_PER_BANK, TOTAL_FEEDER_COUNT);

    // Initialize NVS before we do any other initialization as it may be
    // internally used by various components even if we disable it's usage in
    // the WiFi connection stack.
    ESP_LOGI(TAG, "Initializing NVS");
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_init()) == ESP_ERR_NVS_NO_FREE_PAGES)
    {
      const esp_partition_t *partition =
          esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                   ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
      if (partition != NULL)
      {
        ESP_LOGI(TAG, "Erasing partition %s...", partition->label);
        ESP_ERROR_CHECK(esp_partition_erase_range(partition, 0, partition->size));
        ESP_ERROR_CHECK(nvs_flash_init());
      }
    }

    if(!wifi.start())
    {
        ESP_LOGE(TAG, "Failed to connect to WiFi, rebooting");
        abort();
    }

    asio::io_context io_context;
    GCodeServer server(io_context);

    server.register_command("M115",
        [](std::vector<std::string> args)
        {
            ESP_LOGI(CMD_IMPL_TAG, "Received M115 command, sending reply");
            return "ok FIRMWARE_NAME:Esp32SlottedFeeder Controller PROTOCOL_VERSION:1.0 MACHINE_TYPE:Esp32SlottedFeeder\n";
        });
    server.register_command("M600",
        [](std::vector<std::string> args)
        {
            ESP_LOGI(CMD_IMPL_TAG, "Received M600 command, sending reply");
            return "ok\n";
        });
    server.register_command("M600",
        [](std::vector<std::string> args)
        {
            ESP_LOGI(CMD_IMPL_TAG, "Received M600 command, sending reply");
            return "ok\n";
        });
    io_context.run();
}