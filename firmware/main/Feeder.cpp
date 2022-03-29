/*
 * SPDX-FileCopyrightText: 2022 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

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
               std::shared_ptr<MCP23017> mcp23017, uint8_t channel,
               asio::io_context &context)
    : id_(id), uuid_(uuid), pca9685_(pca9685), mcp23017_(mcp23017),
      channel_(channel), timer_(context)
{
    configure();
}

Feeder::Feeder(std::size_t id, uint32_t uuid, std::shared_ptr<PCA9685> pca9685,
               uint8_t channel, asio::io_context &context)
    : id_(id), uuid_(uuid), pca9685_(pca9685), channel_(channel),
      timer_(context)
{
    configure();
}

void Feeder::configure()
{
    size_t config_size = configsize_;
    nvs_handle_t nvs;
    nvskey_ = "feeder-";
    nvskey_.append(to_hex(uuid_));

    ESP_ERROR_CHECK(nvs_open(NVS_FEEDER_NAMESPACE, NVS_READWRITE, &nvs));
    esp_err_t res = nvs_get_blob(nvs, nvskey_.c_str(), &config_, &config_size);
    if (config_size != configsize_ || res != ESP_OK)
    {
        ESP_LOGW(TAG,
                 "[%s:%zu] Configuration not found or corrupt, rebuilding..",
                 to_hex(uuid_).c_str(), id_);

        memset(&config_, 0, sizeof(feeder_config_t));

        config_.feed_length = FEEDER_MECHANICAL_ADVANCE_LENGTH;
        config_.settle_time = DEFAULT_FEEDER_SETTLE_TIME_MS;
        config_.servo_full_angle = DEFAULT_FEEDER_FULL_ADVANCE_ANGLE;
        config_.servo_half_angle = DEFAULT_FEEDER_FULL_ADVANCE_ANGLE / 2;
        config_.servo_retract_angle = DEFAULT_FEEDER_RETRACT_ANGLE;
        config_.servo_min_pulse = DEFAULT_FEEDER_MIN_PULSE_COUNT;
        config_.servo_max_pulse = DEFAULT_FEEDER_MAX_PULSE_COUNT;

        // If the MCP23017 was not provided ignore feedback by default.
        if (!mcp23017_)
        {
            config_.ignore_feedback = 1;
        }

        ESP_ERROR_CHECK(
            nvs_set_blob(nvs, nvskey_.c_str(), &config_, configsize_));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    }
    nvs_close(nvs);

    ESP_LOGI(TAG,
             "[%s:%zu] Initializing using PCA9685 %p:%d",
             to_hex(uuid_).c_str(), id_, pca9685_.get(), channel_);
    if (mcp23017_ && !config_.ignore_feedback)
    {
        ESP_LOGI(TAG, "[%s:%zu] Feedback enabled using MCP23017 %p:%d",
                 to_hex(uuid_).c_str(), id_, mcp23017_.get(), channel_);
    }

    // move the feeder to retracted position.
    {
        const std::lock_guard<std::mutex> lock(mux_);
        retract_locked();
    }
}

bool Feeder::move(uint8_t distance)
{
    const std::lock_guard<std::mutex> lock(mux_);
    if (is_moving())
    {
        ESP_LOGW(TAG,
                 "[%s:%zu] Feeder is already in motion, rejecting move.",
                 to_hex(uuid_).c_str(), id_);
        return false;
    }

    if (distance == 0)
    {
        movement_ = config_.feed_length;
    }
    else
    {
        movement_ = distance;
    }

    status_ = FEEDER_MOVING;

    // start moving the feeder forward.
    move_locked();

    return true;
}

bool Feeder::post_pick()
{
    if (!is_enabled())
    {
        return false;
    }
    else if (position_ != POSITION_RETRACTED)
    {
        const std::lock_guard<std::mutex> lock(mux_);
        retract_locked();
    }
    return true;
}

std::string Feeder::status()
{
    std::string status = FeederManager::FEEDER_STATUS_CMD;
    status.reserve(128);
    status.append(" N").append(std::to_string(id_));
    status.append(" A").append(std::to_string(config_.servo_full_angle));
    status.append(" B").append(std::to_string(config_.servo_half_angle));
    status.append(" C").append(std::to_string(config_.servo_retract_angle));
    status.append(" F").append(std::to_string(config_.feed_length));
    status.append(" U").append(std::to_string(config_.settle_time));
    status.append(" V").append(std::to_string(config_.servo_min_pulse));
    status.append(" W").append(std::to_string(config_.servo_max_pulse));
    status.append(" X").append(std::to_string(position_));
    status.append(" Y").append(std::to_string(status_));
    status.append(" Z").append(std::to_string(config_.ignore_feedback));    
    return status;
}

bool Feeder::enable()
{
    const std::lock_guard<std::mutex> lock(mux_);
    status_ = FEEDER_IDLE;
    return true;
}

bool Feeder::disable()
{
    const std::lock_guard<std::mutex> lock(mux_);
    status_ = FEEDER_DISABLED;
    return true;
}

void Feeder::configure(uint8_t advance_angle, uint8_t half_advance_angle,
                       uint8_t retract_angle, uint8_t feed_length,
                       uint8_t settle_time, uint8_t min_pulse,
                       uint8_t max_pulse, int8_t ignore_feedback)
{
    bool need_persist = false;
    if (advance_angle)
    {
        config_.servo_full_angle = advance_angle;
        need_persist = true;
    }
    if (half_advance_angle)
    {
        config_.servo_half_angle = half_advance_angle;
        need_persist = true;
    }
    if (retract_angle)
    {
        config_.servo_retract_angle = retract_angle;
        need_persist = true;
    }
    if (feed_length)
    {
        if ((feed_length % 2) == 0)
        {
            config_.feed_length = feed_length;
            need_persist = true;
        }
    }
    if (settle_time)
    {
        config_.settle_time = settle_time;
        need_persist = true;
    }
    if (min_pulse)
    {
        config_.servo_min_pulse = min_pulse;
        need_persist = true;
    }
    if (ignore_feedback >= 0)
    {
        config_.ignore_feedback = ignore_feedback;
        need_persist = true;
    }

    if (need_persist)
    {
        nvs_handle_t nvs;
        // persist the updated configuration
        ESP_ERROR_CHECK(nvs_open(NVS_FEEDER_NAMESPACE, NVS_READWRITE, &nvs));
        ESP_ERROR_CHECK(
            nvs_set_blob(nvs, nvskey_.c_str(), &config_, configsize_));
        ESP_ERROR_CHECK(nvs_commit(nvs));
        nvs_close(nvs);
    }
}

bool Feeder::is_busy()
{
    return !is_enabled() && status_ != FEEDER_IDLE;
}

bool Feeder::is_enabled()
{
    return status_ != FEEDER_DISABLED;
}

bool Feeder::is_moving()
{
    return status_ == FEEDER_MOVING;
}

bool Feeder::is_tensioned()
{
    if (mcp23017_ && !config_.ignore_feedback)
    {
        return mcp23017_->state(channel_);
    }

    // no MCP23017 configured or feedback ignored.
    return true;
}

void Feeder::update(asio::error_code error)
{
    const std::lock_guard<std::mutex> lock(mux_);

    if (error && error != asio::error::operation_aborted)
    {
        ESP_LOGE(TAG, "[%s:%zu] Error received from timer: %s (%d)",
                 to_hex(uuid_).c_str(), id_, error.message().c_str(),
                 error.value());
    }
    else if (is_enabled() && is_moving() && movement_ > 0)
    {
        ESP_LOGI(TAG, "[%s:%zu] Feeder movement remaining: %dmm",
                 to_hex(uuid_).c_str(), id_, movement_);
        // continue moving
        move_locked();
    }
    else if (is_enabled())
    {
        ESP_LOGI(TAG, "[%s:%zu] Feeder movement complete, turning off servo",
                 to_hex(uuid_).c_str(), id_);
        // disable the servo PWM signal.
        pca9685_->off(channel_);

        // reset the feeder status to idle.
        status_ = FEEDER_IDLE;
    }
}

void Feeder::retract_locked()
{
    status_ = FEEDER_MOVING;
    position_ = POSITION_RETRACTED;
    set_servo_angle(config_.servo_retract_angle);
}

void Feeder::move_locked()
{
    if (position_ == POSITION_RETRACTED)
    {
        ESP_LOGI(TAG, "[%s:%zu] Feeder is retracted", to_hex(uuid_).c_str(),
                 id_);
        if (movement_ >= FEEDER_MECHANICAL_ADVANCE_LENGTH)
        {
            ESP_LOGI(TAG, "[%s:%zu] Moving to fully advanced position",
                    to_hex(uuid_).c_str(), id_);
            position_ = POSITION_ADVANCED_FULL;
            movement_ -= FEEDER_MECHANICAL_ADVANCE_LENGTH;
            set_servo_angle(config_.servo_full_angle);
        }
        else if (movement_ >= FEEDER_MECHANICAL_ADVANCE_LENGTH / 2)
        {
            ESP_LOGI(TAG, "[%s:%zu] Moving to half advanced position",
                    to_hex(uuid_).c_str(), id_);
            position_ = POSITION_ADVANCED_HALF;
            movement_ -= (FEEDER_MECHANICAL_ADVANCE_LENGTH / 2);
            set_servo_angle(config_.servo_half_angle);
        }
    }
    else if (position_ == POSITION_ADVANCED_HALF)
    {
        ESP_LOGI(TAG, "[%s:%zu] Feeder is half advanced",
                 to_hex(uuid_).c_str(), id_);
        if (movement_ >= FEEDER_MECHANICAL_ADVANCE_LENGTH / 2)
        {
            ESP_LOGI(TAG, "[%s:%zu] Moving to fully advanced position",
                    to_hex(uuid_).c_str(), id_);
            position_ = POSITION_ADVANCED_FULL;
            movement_ -= (FEEDER_MECHANICAL_ADVANCE_LENGTH / 2);
            set_servo_angle(config_.servo_full_angle);
        }
    }
    else if (position_ == POSITION_ADVANCED_FULL)
    {
        ESP_LOGI(TAG, "[%s:%zu] Feeder fully advanced, retracting",
                 to_hex(uuid_).c_str(), id_);
        retract_locked();
    }
    else
    {
        ESP_LOGE(TAG,
                 "[%s:%zu] Feeder is not in an expect state! position: %d",
                 to_hex(uuid_).c_str(), id_, position_);
    }
}

void Feeder::set_servo_angle(uint8_t angle)
{
    pca9685_->set_servo_angle(channel_, angle, config_.servo_min_pulse,
                              config_.servo_max_pulse);
    // start background timer to shut off the servo or continue
    // movement.
    timer_.expires_from_now(std::chrono::milliseconds(config_.settle_time));
    timer_.async_wait(std::bind(&Feeder::update, this, std::placeholders::_1));
}
