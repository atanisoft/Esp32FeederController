/*
 * SPDX-FileCopyrightText: 2022 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <esp_netif_types.h>
#include "WiFiManager.hxx"

WiFiManager::WiFiManager(const char *const ssid, const char *const password,
                         const char *const hostname)
    : ssid_(ssid), password_(password), hostname_(hostname)
{
}

WiFiManager::~WiFiManager()
{
    // Disconnect our event handlers
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                 &WiFiManager::process_idf_event);
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID,
                                 &WiFiManager::process_idf_event);
}

bool WiFiManager::start()
{
    wifiStatus_ = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t res = esp_event_loop_create_default();
    if (res != ESP_OK && res != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to create default event loop:%s",
                 esp_err_to_name(res));
        abort();
    }
    staIface_ = esp_netif_create_default_wifi_sta();

    // Set the generated hostname prior to connecting to the SSID
    // so that it shows up with the generated hostname instead of
    // the default "Espressif".
    ESP_LOGI(TAG, "Setting hostname to \"%s\".", hostname_.c_str());
    ESP_ERROR_CHECK(esp_netif_set_hostname(staIface_, hostname_.c_str()));

    // Connect our event listeners.
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &WiFiManager::process_idf_event, this);
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                               &WiFiManager::process_idf_event, this);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // This disables storage of SSID details in NVS which has been
    // shown to be problematic at times for the ESP32, it is safer
    // to always pass fresh config and have the ESP32 resolve the
    // details at runtime rather than use a cached set from NVS.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    strcpy(reinterpret_cast<char *>(conf.sta.ssid), ssid_.c_str());
    if (!password_.empty())
    {
        strcpy(reinterpret_cast<char *>(conf.sta.password),
               password_.c_str());
    }

    ESP_LOGI(TAG, "Configuring Station (SSID:%s)", conf.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &conf));

    ESP_LOGI(TAG, "Starting WiFi stack");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(84));

    EventBits_t bits;
    for (uint8_t attempts = 1; attempts <= MAX_CONNECTION_CHECK_ATTEMPTS;
         attempts++)
    {
        bits = xEventGroupWaitBits(wifiStatus_, wifiConnectBitMask_, pdFALSE,
                                   pdTRUE, CONNECTION_CHECK_INTERVAL);
        // If we have connected to the SSID we then are waiting for IP
        // address.
        if (bits & WIFI_CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "[%d/%d] Waiting for IP address assignment.",
                     attempts, MAX_CONNECTION_CHECK_ATTEMPTS);
        }
        else
        {
            // Waiting for SSID connection
            ESP_LOGI(TAG, "[%d/%d] Waiting for SSID connection.",
                     attempts, MAX_CONNECTION_CHECK_ATTEMPTS);
        }
        // Check if have connected to the SSID
        if (bits & WIFI_CONNECTED_BIT)
        {
            // Since we have connected to the SSID we now need to track
            // that we get an IP.
            wifiConnectBitMask_ |= WIFI_GOTIP_BIT;
        }
        // Check if we have received an IP.
        if (bits & WIFI_GOTIP_BIT)
        {
            return true;
        }
    }

    if ((bits & WIFI_CONNECTED_BIT) != WIFI_CONNECTED_BIT)
    {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s.", conf.sta.ssid);
    }
    else if ((bits & WIFI_GOTIP_BIT) != WIFI_GOTIP_BIT)
    {
        ESP_LOGE(TAG, "Timeout waiting for an IP.");
    }

    return false;
}

void WiFiManager::process_idf_event(void *ctx, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    WiFiManager *wifi_mgr = static_cast<WiFiManager *>(ctx);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi station started.");
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        wifi_event_sta_connected_t *data =
            static_cast<wifi_event_sta_connected_t *>(event_data);

        ESP_LOGI(TAG, "Connected to SSID:%s", data->ssid);
        // Set the flag that indictes we are connected to the SSID.
        xEventGroupSetBits(wifi_mgr->wifiStatus_, WIFI_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *data =
            static_cast<wifi_event_sta_disconnected_t *>(event_data);

        // flag to indicate that we should print the reconnecting message.
        bool was_previously_connected = false;

        // capture the current state so we can check if we were already
        // connected with an IP address or still in the connecting phase.
        EventBits_t event_bits = xEventGroupGetBits(wifi_mgr->wifiStatus_);

        // Check if we have already connected, this event can be raised
        // even before we have successfully connected during the SSID
        // connect process.
        if (event_bits & WIFI_CONNECTED_BIT)
        {
            // If we were previously connected and had an IP address we
            // should count that as previously connected, otherwise we will
            // just reconnect.
            was_previously_connected = event_bits & WIFI_GOTIP_BIT;

            ESP_LOGE(TAG, "Lost connection to SSID:%s (reason:%d)",
                     data->ssid, data->reason);
            // Clear the flag that indicates we are connected to the SSID.
            xEventGroupClearBits(wifi_mgr->wifiStatus_, WIFI_CONNECTED_BIT);
            // Clear the flag that indicates we have an IPv4 address.
            xEventGroupClearBits(wifi_mgr->wifiStatus_, WIFI_GOTIP_BIT);
        }

        if (was_previously_connected)
        {
            ESP_LOGI(TAG, "Reconnecting to SSID:%s.",
                     data->ssid);
        }
        else
        {
            ESP_LOGI(TAG, "Connection to SSID:%s (reason:%d) failed",
                     data->ssid, data->reason);
        }
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *data =
            static_cast<ip_event_got_ip_t *>(event_data);

        ESP_LOGI(TAG, "IP address:" IPSTR, IP2STR(&data->ip_info.ip));
        // Set the flag that indictes we have an IPv4 address.
        xEventGroupSetBits(wifi_mgr->wifiStatus_, WIFI_GOTIP_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP)
    {
        ESP_LOGE(TAG, "IP Address lost!");
        // Clear the flag that indicates we have an IPv4 address.
        xEventGroupClearBits(wifi_mgr->wifiStatus_, WIFI_GOTIP_BIT);
    }
}

esp_ip4_addr_t WiFiManager::get_local_ip()
{
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(staIface_, &ip_info));
    return ip_info.ip;
}