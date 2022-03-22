
#include <asio.hpp>
#include <esp_log.h>
#include <cstdint>
#include <nvs.h>
#include <nvs_flash.h>
#include <string>

#include "config.hxx"
#include "GCodeServer.hxx"
#include "FeederManager.hxx"
#include "Feeder.hxx"
#include "Utils.hxx"

Feeder::Feeder(std::size_t id, uint32_t uuid, std::shared_ptr<PCA9685> pca9685,
               uint8_t pca9685_channel, asio::io_context &context)
    : id_(id), uuid_(uuid), pca9685_(pca9685),
      pca9685_channel_(pca9685_channel), timer_(context)
{
    size_t config_size = sizeof(feeder_config_t);
    nvs_handle_t nvs;
    std::string nvs_key = "feeder-";
    nvs_key.append(to_hex(uuid_));

    ESP_LOGI(TAG, "[%zu/%s] Initializing using PCA9685 %p:%d", id_,
             to_hex(uuid_).c_str(), pca9685_.get(), pca9685_channel_);

    ESP_ERROR_CHECK(nvs_open(NVS_FEEDER_NAMESPACE, NVS_READWRITE, &nvs));

    esp_err_t res = nvs_get_blob(nvs, nvs_key.c_str(), &config_, &config_size);
    if (config_size != sizeof(feeder_config_t) || res != ESP_OK)
    {
        ESP_LOGW(TAG,
                 "[%zu/%s] Configuration not found or corrupt, rebuilding..",
                 id_, to_hex(uuid_).c_str());

        memset(&config_, 0, sizeof(feeder_config_t));

        config_.feed_length = FEEDER_MECHANICAL_ADVANCE_LENGTH;
        config_.settle_time = DEFAULT_FEEDER_SETTLE_TIME_MS;
        config_.servo_full_angle = DEFAULT_FEEDER_FULL_ADVANCE_ANGLE;
        config_.servo_half_angle = DEFAULT_FEEDER_FULL_ADVANCE_ANGLE / 2;
        config_.servo_retract_angle = DEFAULT_FEEDER_RETRACT_ANGLE;
        config_.servo_min_pulse = DEFAULT_FEEDER_MIN_PULSE_COUNT;
        config_.servo_max_pulse = DEFAULT_FEEDER_MAX_PULSE_COUNT;

        ESP_ERROR_CHECK(
            nvs_set_blob(nvs, nvs_key.c_str(), &config_,
                         sizeof(feeder_config_t)));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    }
    nvs_close(nvs);
    retract();
}

GCodeServer::command_return_type Feeder::retract()
{
    return std::make_pair(true, "");
}

GCodeServer::command_return_type Feeder::move()
{
    if (status_ == FEEDER_DISABLED)
    {
        return std::make_pair(false, "Feeder not enabled!");
    }
    else if (status_ == FEEDER_MOVING)
    {
        movement_ += config_.feed_length;
        return std::make_pair(true, "");
    }

    return std::make_pair(true, "");
}

GCodeServer::command_return_type Feeder::post_pick()
{
    return std::make_pair(true, "");
}

GCodeServer::command_return_type Feeder::status()
{
    return std::make_pair(true, "");
}

GCodeServer::command_return_type Feeder::enable()
{
    return std::make_pair(true, "");
}

GCodeServer::command_return_type Feeder::disable()
{
    return std::make_pair(true, "");
}

GCodeServer::command_return_type Feeder::configure(uint8_t advance_angle,
                                                   uint8_t half_advance_angle,
                                                   uint8_t retract_angle,
                                                   uint8_t feed_length,
                                                   uint8_t settle_time,
                                                   uint8_t min_pulse,
                                                   uint8_t max_pulse)
{
    
    return std::make_pair(true, "");
}

void Feeder::update(asio::error_code error)
{
    if (status_ != FEEDER_DISABLED)
    {
        //if (movement_)
        //if (position_ != target_)
    }
    timer_.expires_from_now(std::chrono::milliseconds(config_.settle_time));
    timer_.async_wait(std::bind(&Feeder::update, shared_from_this(),
                                std::placeholders::_1));
}