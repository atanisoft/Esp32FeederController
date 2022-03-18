
#include <algorithm>
#include <asio.hpp>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <string>
#include "config.hxx"
#include "FeederManager.hxx"
#include "WiFiManager.hxx"
#include "GCodeServer.hxx"
#include "Utils.hxx"
#include "SocInfo.hxx"

static constexpr const char *const TAG = "main";
static constexpr const char *const CMD_IMPL_TAG = "command";

static WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD, WIFI_HOSTNAME);
static FeederManager feeder_mgr;

extern "C" void app_main()
{
    configure_log_levels();

    ESP_LOGI(TAG, "Esp32SlottedFeeder v0.0 Initializing");
    SocInfo::print_soc_info();

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
    feeder_mgr.start(server);

    server.register_command("M115",
        [](std::vector<std::string> args)
        {
            ESP_LOGI(CMD_IMPL_TAG, "Received M115 command, sending reply");
            return "ok FIRMWARE_NAME:Esp32SlottedFeeder Controller PROTOCOL_VERSION:1.0 MACHINE_TYPE:Esp32SlottedFeeder\n";
        });

    // Create a timer that fires roughly every 30 seconds to report heap usage
    asio::system_timer heap_timer(io_context, std::chrono::seconds(30));
    std::function<void(asio::error_code)> heap_monitor =
    [&](asio::error_code ec)
    {
        static constexpr const char * const TAG = "heap_mon";
        if (!ec)
        {
            ESP_LOGI(TAG, "Heap: %.2fkB / %.2fkB",
                    heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024.0f,
                    heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024.0f);
#if CONFIG_SPIRAM_SUPPORT
            ESP_LOGI(TAG, "PSRAM: %.2fkB / %.2fkB",
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024.0f,
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024.0f);
#endif // CONFIG_SPIRAM_SUPPORT
            heap_timer.expires_from_now(std::chrono::seconds(30));
            heap_timer.async_wait(heap_monitor);
        }
    };
    // start the heap monitor timer
    heap_monitor({});

    io_context.run();
}