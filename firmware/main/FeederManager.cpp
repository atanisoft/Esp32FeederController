
#include <esp_log.h>
#include <stdint.h>
#include <string>

#include "config.hxx"
#include "GCodeServer.hxx"
#include "FeederManager.hxx"

using std::string;
using std::transform;

FeederManager::FeederManager()
{
}

void FeederManager::start(GCodeServer &server)
{
    ESP_LOGI(TAG, "Banks:%zu, Feeders per bank:%zu, Total Feeders:%zu",
             FEEDER_BANK_COUNT, MAX_FEEDERS_PER_BANK, TOTAL_FEEDER_COUNT);
    server.register_command(FEEDER_MOVE_CMD,
                            std::bind(&FeederManager::feeder_move, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_POST_PICK_CMD,
                            std::bind(&FeederManager::feeder_post_pick, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_STATUS_CMD,
                            std::bind(&FeederManager::feeder_status, this,
                                      std::placeholders::_1));
    server.register_command(FEEDER_CONFIGURE_CMD,
                            std::bind(&FeederManager::feeder_configure, this,
                                      std::placeholders::_1));
}

template <typename T>
bool FeederManager::extract_arg(string arg_letter,
                                FeederManager::cmd_args args,
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

FeederManager::cmd_reply FeederManager::feeder_move(FeederManager::cmd_args args)
{
    ESP_LOGI(TAG, "feeder move request received");
    uint8_t bank = -1, feeder = -1;

    if (!extract_arg("T", args, bank))
    {
        return std::make_pair(false, "Missing bank ID");
    }
    if (!extract_arg("N", args, bank))
    {
        return std::make_pair(false, "Missing feeder ID");
    }
    ESP_LOGI(TAG, "Moving Feeder(%d:%d) forward", bank, feeder);

    return std::make_pair(true, "");
}

FeederManager::cmd_reply FeederManager::feeder_post_pick(FeederManager::cmd_args args)
{
    ESP_LOGI(TAG, "feeder post pick request received");
    uint8_t bank = -1, feeder = -1;

    if (!extract_arg("T", args, bank))
    {
        return std::make_pair(false, "Missing bank ID");
    }
    if (!extract_arg("N", args, bank))
    {
        return std::make_pair(false, "Missing feeder ID");
    }
    ESP_LOGI(TAG, "Post Pick Feeder(%d:%d)", bank, feeder);

    return std::make_pair(true, "");
}

FeederManager::cmd_reply FeederManager::feeder_status(FeederManager::cmd_args args)
{
    ESP_LOGI(TAG, "feeder status request received");
    uint8_t bank = -1, feeder = -1;

    if (!extract_arg("T", args, bank))
    {
        return std::make_pair(false, "Missing bank ID");
    }
    if (!extract_arg("N", args, bank))
    {
        return std::make_pair(false, "Missing feeder ID");
    }

    ESP_LOGI(TAG, "Feeder(%d:%d) status", bank, feeder);

    return std::make_pair(true, "");
}

FeederManager::cmd_reply FeederManager::feeder_configure(FeederManager::cmd_args args)
{
    ESP_LOGI(TAG, "feeder reconfigure request received");
    uint8_t bank = -1, feeder = -1;
    int16_t advance_angle = 0, half_advance_angle = 0, retract_angle = 0;
    uint16_t feed_length = 0, settle_time = 0, min_pulse = 0, max_pulse = 0;

    if (!extract_arg("T", args, bank))
    {
        return std::make_pair(false, "Missing bank ID");
    }
    if (!extract_arg("N", args, bank))
    {
        return std::make_pair(false, "Missing feeder ID");
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

    ESP_LOGI(TAG, "Feeder(%d:%d) reconfigure", bank, feeder);
    return std::make_pair(true, "");
}