/*
 * SPDX-FileCopyrightText: 2022 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <algorithm>
#include <asio.hpp>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_pthread.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <string>
#include "config.hxx"
#include "FeederManager.hxx"
#include "WiFiManager.hxx"
#include "GCodeServer.hxx"
#include "Utils.hxx"
#include "SocInfo.hxx"

static WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD, WIFI_HOSTNAME);

static void worker_task(asio::io_context &context)
{
    const char *const TAG = "worker";
    try
    {
        context.run();
    }
    catch (const std::exception &e)
    {
        ESP_LOGE(TAG, "Exception occured during client thread execution %s",
                 e.what());
    }
    catch (...)
    {
        ESP_LOGE(TAG, "Unknown exception");
    }
}

extern "C" void app_main()
{
    const esp_app_desc_t *app_data = esp_ota_get_app_description();
    const esp_partition_t *running_from = esp_ota_get_running_partition();
    const char *const TAG = "main";
    esp_chip_info_t chip_info;

    esp_chip_info(&chip_info);
    configure_log_levels();

    ESP_LOGI(TAG, "%s %s Initializing", app_data->project_name, app_data->version);
    ESP_LOGI(TAG, "Compiled on %s %s using IDF %s", app_data->date,
             app_data->time, app_data->idf_ver);
    ESP_LOGI(TAG, "Running from: %s", running_from->label);
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

    if (!wifi.start())
    {
        ESP_LOGE(TAG, "Failed to connect to WiFi, rebooting");
        abort();
    }

    asio::io_context context;
    GCodeServer gcode_server(context, wifi.get_local_ip());
    FeederManager feeder_mgr(gcode_server, context);
    asio::system_timer heap_timer(context, std::chrono::seconds(30));
    std::function<void(asio::error_code)> heap_monitor =
        [&](asio::error_code ec)
    {
        const char *const HEAP_TAG = "heap_mon";
        if (!ec)
        {
            ESP_LOGI(HEAP_TAG, "Heap: %.2fkB / %.2fkB",
                     heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024.0f,
                     heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024.0f);
#if CONFIG_SPIRAM_SUPPORT
            ESP_LOGI(HEAP_TAG, "PSRAM: %.2fkB / %.2fkB",
                     heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024.0f,
                     heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024.0f);
#endif // CONFIG_SPIRAM_SUPPORT
            heap_timer.expires_from_now(std::chrono::seconds(30));
            heap_timer.async_wait(heap_monitor);
        }
    };

    // start the heap monitor task, this is necessary to kick start the task
    // in the background.
    heap_monitor({});

    std::vector<std::thread> workers;
    // Create two workers per core to increase concurrency of execution.
    const int worker_count = chip_info.cores * 2;
    ESP_LOGI(TAG, "Creating %d worker threads", worker_count);
    for (int id = 0; id < worker_count; id++)
    {
        // std::thread does not expose options for thread name, stack size or
        // pinning to a core. ESP-IDF provides a pthread extension for this
        // which must be called prior to thread creation.
        auto cfg = esp_pthread_get_default_config();
        std::string name = "worker-";
        name.append(std::to_string(id));
        cfg.thread_name = name.c_str();
        cfg.pin_to_core = id;
        esp_pthread_set_cfg(&cfg);

        // Create the worker thread which passes in the asio::io_context by
        // reference. Internally the asio::io_context object will manage
        // locking as required.
        workers.emplace_back(std::thread(worker_task, std::ref(context)));
    }
    ESP_LOGI(TAG, "%s Ready!", app_data->project_name);

    // Wait for all workers to exit.
    for (auto &worker : workers)
    {
        worker.join();
    }
}