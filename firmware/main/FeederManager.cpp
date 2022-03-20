
#include <asio.hpp>
#include <asio/bind_executor.hpp>
#include <esp_log.h>
#include <cstdint>
#include <nvs.h>
#include <nvs_flash.h>
#include <string>

#include "config.hxx"
#include "GCodeServer.hxx"
#include "FeederManager.hxx"
#include "I2Cbus.hxx"

using std::string;
using std::transform;

FeederManager::FeederManager(GCodeServer &server)
    : i2c_(getI2C(I2C_NUM_0))
{
    size_t config_size = sizeof(feeder_manager_config_t);
    feeder_manager_config_t config;
    nvs_handle_t nvs;

    server.register_command(FEEDER_MOVE_CMD,
                            std::bind(&FeederManager::feeder_move, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_POST_PICK_CMD,
                            std::bind(&FeederManager::feeder_post_pick, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_STATUS_CMD,
                            std::bind(&FeederManager::feeder_status, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_GROUP_CONFIGURE_CMD,
                            std::bind(&FeederManager::feeder_group_configure, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_CONFIGURE_CMD,
                            std::bind(&FeederManager::feeder_configure, this,
                                      std::placeholders::_1));

    ESP_ERROR_CHECK(nvs_open(NVS_FEEDER_NAMESPACE, NVS_READWRITE, &nvs));

    esp_err_t res = nvs_get_blob(nvs, NVS_FEEDER_MGR_CFG_KEY, &config,
                                 &config_size);
    if (config_size != sizeof(feeder_manager_config_t) || res != ESP_OK)
    {
        ESP_LOGW(TAG, "Configuration not found or corrupt, reinitializing..");

        memset(&config, 0, sizeof(feeder_manager_config_t));

        config.bank_count = FEEDER_BANK_COUNT;
        esp_fill_random(&config.feeder_group_id, MAX_FEEDER_GROUPS);

        ESP_ERROR_CHECK(
            nvs_set_blob(nvs, NVS_FEEDER_MGR_CFG_KEY, &config,
                         sizeof(feeder_manager_config_t)));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    }
    nvs_close(nvs);

    ESP_LOGI(TAG, "Initializing I2C Bus");
    i2c_.begin(I2C_SDA_PIN_NUM, I2C_SCL_PIN_NUM, I2C_BUS_SPEED);

    for (size_t idx = 0; idx < config.bank_count; idx++)
    {
        banks_.push_back(
            std::make_unique<FeederBank>(i2c_, idx,
                                         config.feeder_group_id[idx]));

        if (!banks_[idx]->load_configuration())
        {
            ESP_LOGE(TAG,
                     "Failed to load and configure feeders in bank: %d, "
                     "please reconfigure via GCode: %s. Individual feeders "
                     "may also require reconfiguration via GCode: %s",
                     idx, FEEDER_GROUP_CONFIGURE_CMD, FEEDER_CONFIGURE_CMD);
        }
    }
    ESP_LOGI(TAG, "Banks:%zu, Feeders per bank:%zu, Total Feeders:%zu",
             banks_.size(), MAX_FEEDERS_PER_BANK,
             banks_.size() * MAX_FEEDERS_PER_BANK);
}

template <typename T>
bool FeederManager::extract_arg(string arg_letter,
                                GCodeServer::command_args args,
                                T &value)
{
    auto ent = std::find_if(args.begin(), args.end(),
                            [arg_letter](auto &arg)
                            {
                                string arg_upper = arg;
                                transform(arg_upper.begin(), arg_upper.end(),
                                          arg_upper.begin(), ::toupper);
                                return arg.rfind(arg_letter, 0) != string::npos;
                            });
    if (ent != args.end())
    {
        value = std::stoi(ent->substr(arg_letter.length()));
        return true;
    }
    return false;
}

GCodeServer::command_return_type FeederManager::feeder_move(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder move request received");
    uint8_t bank = -1, feeder = -1;

    if (!extract_arg("T", args, bank) || bank > banks_.size())
    {
        return std::make_pair(false, "Missing/invalid bank ID");
    }
    if (!extract_arg("N", args, feeder) || feeder > MAX_FEEDERS_PER_BANK)
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    return banks_[bank]->move(feeder);
}

GCodeServer::command_return_type FeederManager::feeder_post_pick(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder post pick request received");
    uint8_t bank = -1, feeder = -1;

    if (!extract_arg("T", args, bank) || bank > banks_.size())
    {
        return std::make_pair(false, "Missing/invalid bank ID");
    }
    if (!extract_arg("N", args, feeder) || feeder > MAX_FEEDERS_PER_BANK)
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    return banks_[bank]->post_pick(feeder);
}

GCodeServer::command_return_type FeederManager::feeder_status(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder status request received");
    uint8_t bank = -1, feeder = -1;

    if (!extract_arg("T", args, bank) || bank > banks_.size())
    {
        return std::make_pair(false, "Missing/invalid bank ID");
    }
    if (!extract_arg("N", args, feeder) || feeder > MAX_FEEDERS_PER_BANK)
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    return banks_[bank]->status(feeder);
}

GCodeServer::command_return_type FeederManager::feeder_group_configure(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder group reconfigure request received");
    uint8_t bank = -1, pca9685_a = -1, pca9685_b = -1;
    uint32_t pca9685_a_freq = -1, pca9685_b_freq = -1;

    if (!extract_arg("T", args, bank) || bank > banks_.size())
    {
        return std::make_pair(false, "Missing/invalid bank ID");
    }
    if (!extract_arg("A", args, pca9685_a))
    {
        return std::make_pair(false, "Missing PCA9685 A address");
    }
    if (!extract_arg("B", args, pca9685_b))
    {
        return std::make_pair(false, "Missing PCA9685 B address");
    }
    if (!extract_arg("C", args, pca9685_a_freq))
    {
        return std::make_pair(false, "Missing PCA9685 A frequency");
    }
    if (!extract_arg("D", args, pca9685_b_freq))
    {
        return std::make_pair(false, "Missing PCA9685 B frequency");
    }

    return banks_[bank]->configure(pca9685_a, pca9685_b, pca9685_a_freq,
                                   pca9685_b_freq);
}

GCodeServer::command_return_type FeederManager::feeder_configure(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder reconfigure request received");
    uint8_t bank = -1, feeder = -1;
    int16_t advance_angle = 0, half_advance_angle = 0, retract_angle = 0;
    uint16_t feed_length = 0, settle_time = 0, min_pulse = 0, max_pulse = 0;

    if (!extract_arg("T", args, bank) || bank > banks_.size())
    {
        return std::make_pair(false, "Missing/invalid bank ID");
    }
    if (!extract_arg("N", args, feeder) || feeder > MAX_FEEDERS_PER_BANK)
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    if (!extract_arg("A", args, advance_angle))
    {
        return std::make_pair(false, "Missing advance_angle");
    }
    if (!extract_arg("B", args, half_advance_angle))
    {
        return std::make_pair(false, "Missing half_advance_angle");
    }
    if (!extract_arg("C", args, retract_angle))
    {
        return std::make_pair(false, "Missing retract_angle");
    }
    if (!extract_arg("F", args, feed_length))
    {
        return std::make_pair(false, "Missing feed_length");
    }
    if (!extract_arg("U", args, settle_time))
    {
        return std::make_pair(false, "Missing settle_time");
    }
    if (!extract_arg("V", args, min_pulse))
    {
        return std::make_pair(false, "Missing min_pulse");
    }
    if (!extract_arg("W", args, max_pulse))
    {
        return std::make_pair(false, "Missing max_pulse");
    }

    return banks_[bank]->configure(feeder, advance_angle, half_advance_angle,
                                   retract_angle, feed_length, settle_time,
                                   min_pulse, max_pulse);
}

FeederManager::FeederBank::FeederBank(I2C_t &i2c, uint8_t bank, uint32_t id)
    : bank_(bank), id_(id), pca9685_{{i2c}, {i2c}}
{
}

bool FeederManager::FeederBank::load_configuration()
{
    size_t config_size = sizeof(feeder_bank_config_t);
    nvs_handle_t nvs;

    std::string nvs_key = "bank-";
    nvs_key.append(std::to_string(id_));

    ESP_ERROR_CHECK(nvs_open(NVS_FEEDER_NAMESPACE, NVS_READWRITE, &nvs));

    esp_err_t res = ESP_ERROR_CHECK_WITHOUT_ABORT(
        nvs_get_blob(nvs, nvs_key.c_str(), &config_, &config_size));
    if (config_size != sizeof(feeder_bank_config_t) || res != ESP_OK)
    {
        ESP_LOGW(TAG,
                 "[%d] Configuration not found or corrupt, reinitializing..",
                 id_);

        memset(&config_, 0, sizeof(feeder_bank_config_t));

        config_.pca9685_address[0] = 0x40 + (bank_ * 2);
        config_.pca9685_address[1] = 0x41 + (bank_ * 2);
        config_.pca9685_frequency[0] = 50;
        config_.pca9685_frequency[1] = 50;
        config_.feedback_address[0] = 0x20 + (bank_ * 2);
        config_.feedback_address[1] = 0x21 + (bank_ * 2);
        for (std::size_t idx = 0; idx < MAX_FEEDERS_PER_BANK; idx++)
        {
            config_.feeder_config[idx].full_angle = DEFAULT_FULL_ANGLE;
            config_.feeder_config[idx].half_angle = DEFAULT_HALF_ANGLE;
            config_.feeder_config[idx].retract_angle = DEFAULT_RETRACT_ANGLE;
            config_.feeder_config[idx].feed_length = FEEDER_MECHANICAL_ADVANCE_LENGTH;
            config_.feeder_config[idx].settle_time = DEFAULT_SETTLE_TIME_MS;
            config_.feeder_config[idx].min_pulse = DEFAULT_MIN_PULSE_COUNT;
            config_.feeder_config[idx].max_pulse = DEFAULT_MAX_PULSE_COUNT;
        }

        ESP_ERROR_CHECK(
            nvs_set_blob(nvs, nvs_key.c_str(), &config_,
                         sizeof(feeder_bank_config_t)));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    }
    nvs_close(nvs);

    // display configuration
    ESP_LOGI(TAG, "[%d] PCA9685(a): %02x %d", id_, config_.pca9685_address[0],
             config_.pca9685_frequency[0]);
    ESP_LOGI(TAG, "[%d] PCA9685(b): %02x %d", id_, config_.pca9685_address[1],
             config_.pca9685_frequency[1]);

    bool ready[PCA965_COUNT];
    for (std::size_t idx = 0; idx < PCA965_COUNT; idx++)
    {
        esp_err_t res =
            pca9685_[idx].configure(config_.pca9685_address[idx],
                                    config_.pca9685_frequency[idx]);
        if (res != ESP_OK)
        {
            ESP_LOGW(TAG, "[%d] PCA9685 %02x not detected or configured!", id_,
                     config_.pca9685_address[idx]);
            ready[idx] = false;
        }
    }

    for (std::size_t idx = 0; idx < MAX_FEEDERS_PER_BANK; idx++)
    {
        status_[idx] = FEEDER_DISABLED;
        position_[idx] = target_[idx] = POSITION_UNKNOWN;
        movement_[idx] = 0;
        if (ready[idx / PCA9685::NUM_CHANNELS])
        {
            retract(idx);
        }
    }

    return ready;
}

GCodeServer::command_return_type FeederManager::FeederBank::retract(uint8_t feeder)
{
    std::size_t channel = feeder % PCA9685::NUM_CHANNELS;
    std::size_t pca9685_idx = feeder / PCA9685::NUM_CHANNELS;
    auto angle = config_.feeder_config[feeder].retract_angle;
    auto min_pulse = config_.feeder_config[feeder].min_pulse;
    auto max_pulse = config_.feeder_config[feeder].max_pulse;
    auto settle_time = config_.feeder_config[feeder].settle_time;

    if (status_[feeder] == FEEDER_MOVING)
    {
        return std::make_pair(false, "Feeder is already in motion!");
    }

    target_[feeder] = POSITION_RETRACT;
    if (status_[feeder] != FEEDER_DISABLED)
    {
        status_[feeder] = FEEDER_RETRACTING;
    }
    if (pca9685_[pca9685_idx].set_servo_angle(channel, angle, min_pulse, max_pulse) != ESP_OK)
    {
        return std::make_pair(false, "Feeder reported a movement failure!");
    }

    // TODO: move the turn off block below to be handled in the background!
    vTaskDelay(pdMS_TO_TICKS(settle_time));

    if (pca9685_[pca9685_idx].off(channel) != ESP_OK)
    {
        return std::make_pair(false, "Feeder reported a movement failure!");
    }

    return std::make_pair(true, "");
}

GCodeServer::command_return_type FeederManager::FeederBank::move(uint8_t feeder)
{
    std::size_t channel = feeder % PCA9685::NUM_CHANNELS;
    std::size_t pca9685_idx = feeder / PCA9685::NUM_CHANNELS;

    if (status_[feeder] == FEEDER_DISABLED)
    {
        return std::make_pair(false, "Feeder not enabled!");
    }
    else if (status_[feeder] == FEEDER_MOVING)
    {
        movement_[feeder] += config_.feeder_config[feeder].feed_length;
        return std::make_pair(true, "");
    }

    uint16_t angle = config_.feeder_config[feeder].full_angle;
    auto min_pulse = config_.feeder_config[feeder].min_pulse;
    auto max_pulse = config_.feeder_config[feeder].max_pulse;
    auto settle_time = config_.feeder_config[feeder].settle_time;
    movement_[feeder] += config_.feeder_config[feeder].feed_length;
    target_[feeder] = POSITION_ADVANCED_FULL;
    status_[feeder] = FEEDER_MOVING;
    if (movement_[feeder] < FEEDER_MECHANICAL_ADVANCE_LENGTH)
    {
        target_[feeder] = POSITION_ADVANCED_HALF;
        angle = config_.feeder_config[feeder].half_angle;
    }
    auto res = pca9685_[pca9685_idx].set_servo_angle(channel, angle, min_pulse, max_pulse);
    if (res != ESP_OK)
    {
        return std::make_pair(false, "Failed to move servo to retraction angle");
    }

    // TODO: move the turn off block below to be handled in the background!
    vTaskDelay(pdMS_TO_TICKS(settle_time));

    if (pca9685_[pca9685_idx].off(channel) != ESP_OK)
    {
        return std::make_pair(false, "Feeder reported a movement failure!");
    }
    return std::make_pair(true, "");
}

GCodeServer::command_return_type FeederManager::FeederBank::post_pick(uint8_t feeder)
{
    return std::make_pair(true, "");
}

GCodeServer::command_return_type FeederManager::FeederBank::status(uint8_t feeder)
{
    return std::make_pair(true, "");
}

GCodeServer::command_return_type FeederManager::FeederBank::configure(uint8_t feeder,
                                                                      uint8_t advance_angle,
                                                                      uint8_t half_advance_angle,
                                                                      uint8_t retract_angle,
                                                                      uint8_t feed_length,
                                                                      uint8_t settle_time,
                                                                      uint8_t min_pulse,
                                                                      uint8_t max_pulse)
{
    if (feed_length % 2)
    {
        return std::make_pair(false, "Feed length must be a multiple of 2.");
    }
    return std::make_pair(true, "");
}

GCodeServer::command_return_type FeederManager::FeederBank::configure(uint8_t pca9685_a,
                                                                      uint8_t pca9685_b,
                                                                      uint32_t pca9685_a_freq,
                                                                      uint32_t pca9685_b_freq)
{
    return std::make_pair(true, "");
}
