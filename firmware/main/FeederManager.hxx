#pragma once

#include "config.hxx"
#include "GCodeServer.hxx"

class FeederManager
{
public:
    FeederManager(const FeederManager &) = delete;
    FeederManager &operator=(const FeederManager &) = delete;

    FeederManager()
    {
    }

    void start(GCodeServer &server)
    {
        ESP_LOGI(TAG, "Banks:%zu, Feeders per bank:%zu, Total Feeders:%zu",
                 FEEDER_BANK_COUNT, MAX_FEEDERS_PER_BANK, TOTAL_FEEDER_COUNT);
        server.register_command("M600",
            std::bind(&FeederManager::feeder_move, this, std::placeholders::_1));
        server.register_command("M601",
            std::bind(&FeederManager::feeder_post_pick, this, std::placeholders::_1));
    }

private:
    /// Maximum number of feeders to configure per bank.
    static constexpr size_t MAX_FEEDERS_PER_BANK = 48;

    /// Total number of feeders to configure.
    static constexpr size_t TOTAL_FEEDER_COUNT = MAX_FEEDERS_PER_BANK * FEEDER_BANK_COUNT;

    /// Log tag to use for this class.
    static constexpr const char *TAG = "feeder_mgr";

    std::string feeder_move(std::vector<std::string> args)
    {
        ESP_LOGI(TAG, "M600 received");
        return "ok\n";
    }

    std::string feeder_post_pick(std::vector<std::string> args)
    {
        ESP_LOGI(TAG, "M601 received");
        return "ok\n";
    }

};