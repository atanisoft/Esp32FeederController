
#include <asio.hpp>
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

FeederManager::FeederManager(GCodeServer &server, asio::io_context &context)
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
    server.register_command(FEEDER_ENABLE_CMD,
                            std::bind(&FeederManager::feeder_enable, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_DISABLE_CMD,
                            std::bind(&FeederManager::feeder_disable, this,
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

        esp_fill_random(&config.feeder_uuid, MAX_FEEDER_COUNT);

        ESP_ERROR_CHECK(
            nvs_set_blob(nvs, NVS_FEEDER_MGR_CFG_KEY, &config,
                         sizeof(feeder_manager_config_t)));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    }
    nvs_close(nvs);

    ESP_LOGI(TAG, "Initializing I2C Bus");
    i2c_.begin(I2C_SDA_PIN_NUM, I2C_SCL_PIN_NUM, I2C_BUS_SPEED);

    for (uint8_t addr = PCA9685_BASE_ADDRESS;
         addr < (PCA9685_BASE_ADDRESS + MAX_PCA9685_COUNT);
         addr++)
    {
        if (i2c_.testConnection(addr) == ESP_OK)
        {
            pca9685_.push_back(std::make_shared<PCA9685>(i2c_));
            if (pca9685_.back()->configure(addr, PCA9685_FREQUENCY) != ESP_OK)
            {
                ESP_LOGW(TAG, "PCA9685(%02x) configuration failed!", addr);
                pca9685_.pop_back();
            }
            else
            {
                ESP_LOGI(TAG, "PCA9685(%02x/%p) configured for use.", addr,
                         pca9685_.back().get());
            }
        }
        else
        {
            ESP_LOGW(TAG, "PCA9685(%02x) was not detected.", addr);
        }
    }
    ESP_LOGI(TAG, "Detected PCA9685 devices:%zu", pca9685_.size());

    for (uint8_t addr = MCP23017_BASE_ADDRESS;
         addr < (MCP23017_BASE_ADDRESS + MAX_MCP23017_COUNT);
         addr++)
    {
        if (i2c_.testConnection(addr) == ESP_OK)
        {
            mcp23017_.push_back(std::make_shared<MCP23017>(i2c_, context));
            if (mcp23017_.back()->configure(addr) != ESP_OK)
            {
                ESP_LOGW(TAG, "MCP23017(%02x) configuration failed!", addr);
                mcp23017_.pop_back();
            }
            else
            {
                ESP_LOGI(TAG, "MCP23017(%02x/%p) configured for use.", addr,
                         mcp23017_.back().get());
            }
        }
        else
        {
            ESP_LOGW(TAG, "MCP23017(%02x) was not detected!", addr);
        }
    }

    // calculate how many feeders we should configured based on the the number
    // of PCA9685 chips that were detected and configured.
    std::size_t available_feeder_count =
        std::min(MAX_FEEDER_COUNT, pca9685_.size() * PCA9685::NUM_CHANNELS);
    for (size_t idx = 0; idx < available_feeder_count; idx++)
    {
        auto expander_index = idx / PCA9685::NUM_CHANNELS;
        auto expander_channel = idx % PCA9685::NUM_CHANNELS;
        uint32_t uuid = config.feeder_uuid[idx];
        if (mcp23017_.size() > expander_index)
        {
            feeders_.push_back(
                std::make_shared<Feeder>(idx + 1, uuid,
                                        pca9685_[expander_index],
                                        mcp23017_[expander_index],
                                        expander_channel, context));
        }
        else
        {
            feeders_.push_back(
                std::make_shared<Feeder>(idx + 1, uuid,
                                        pca9685_[expander_index],
                                        expander_channel, context));
        }
    }
    ESP_LOGI(TAG, "Configured Feeders:%zu", feeders_.size());
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
    uint8_t feeder = -1;

    if (!extract_arg("N", args, feeder) || feeder > feeders_.size())
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    return feeders_[feeder]->move();
}

GCodeServer::command_return_type FeederManager::feeder_post_pick(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder post pick request received");
    uint8_t feeder = -1;

    if (!extract_arg("N", args, feeder) || feeder > feeders_.size())
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    return feeders_[feeder]->post_pick();
}

GCodeServer::command_return_type FeederManager::feeder_status(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder status request received");
    uint8_t feeder = -1;

    if (!extract_arg("N", args, feeder) || feeder > feeders_.size())
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }

    return feeders_[feeder]->status();
}

GCodeServer::command_return_type FeederManager::feeder_enable(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder enable request received");
    uint8_t feeder = -1;
    if (!extract_arg("N", args, feeder) || feeder > feeders_.size())
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    return feeders_[feeder]->enable();
}

GCodeServer::command_return_type FeederManager::feeder_disable(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder disable request received");
    uint8_t feeder = -1;
    if (!extract_arg("N", args, feeder) || feeder > feeders_.size())
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    return feeders_[feeder]->disable();
}

GCodeServer::command_return_type FeederManager::feeder_configure(GCodeServer::command_args args)
{
    ESP_LOGI(TAG, "feeder reconfigure request received");
    uint8_t feeder = -1;
    int16_t advance_angle = 0, half_advance_angle = 0, retract_angle = 0;
    uint16_t feed_length = 0, settle_time = 0, min_pulse = 0, max_pulse = 0;

    if (!extract_arg("N", args, feeder) || feeder > feeders_.size())
    {
        return std::make_pair(false, "Missing/invalid feeder ID");
    }
    if (extract_arg("F", args, feed_length) && feed_length % 2)
    {
        return std::make_pair(false, "Feed length must be a multiple of 2.");
    }
    extract_arg("A", args, advance_angle);
    extract_arg("B", args, half_advance_angle);
    extract_arg("C", args, retract_angle);
    extract_arg("U", args, settle_time);
    extract_arg("V", args, min_pulse);
    extract_arg("W", args, max_pulse);
    return feeders_[feeder]->configure(advance_angle, half_advance_angle,
                                       retract_angle, feed_length, settle_time,
                                       min_pulse, max_pulse);
}
