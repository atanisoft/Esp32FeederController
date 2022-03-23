#pragma once

#include <asio.hpp>
#include <map>

#include "config.hxx"
#include "PCA9685.hxx"
#include "MCP23017.hxx"
#include "GCodeServer.hxx"

class Feeder : public std::enable_shared_from_this<Feeder>
{
public:
    /// Constructor.
    ///
    /// @param uuid Unique identifier for this feeder.
    /// @param pca9685 @ref PCA9685 to use for this feeder.
    /// @param mcp23017 @ref MCP23017 to use for this feeder.
    /// @param channel @ref IO Expander channel assigned to this feeder.
    Feeder(std::size_t id, uint32_t uuid, std::shared_ptr<PCA9685> pca9685,
           std::shared_ptr<MCP23017> mcp23017, uint8_t channel,
           asio::io_context &context);

    /// Constructor.
    ///
    /// @param uuid Unique identifier for this feeder.
    /// @param pca9685 @ref PCA9685 to use for this feeder.
    /// @param channel @ref IO Expander channel assigned to this feeder.
    Feeder(std::size_t id, uint32_t uuid, std::shared_ptr<PCA9685> pca9685,
           uint8_t channel, asio::io_context &context);

    GCodeServer::command_return_type retract();

    GCodeServer::command_return_type move();

    GCodeServer::command_return_type post_pick();

    GCodeServer::command_return_type status();

    GCodeServer::command_return_type enable();
    
    GCodeServer::command_return_type disable();

    GCodeServer::command_return_type configure(uint8_t advance_angle,
                                               uint8_t half_advance_angle,
                                               uint8_t retract_angle,
                                               uint8_t feed_length,
                                               uint8_t settle_time,
                                               uint8_t min_pulse,
                                               uint8_t max_pulse);
private:
    /// Log tag to use for this class.
    static constexpr const char *const TAG = "feeder";

    /// Movement defined by mechanicial limitations.
    static constexpr uint8_t FEEDER_MECHANICAL_ADVANCE_LENGTH = 4;

    typedef struct
    {
        uint8_t feed_length;
        uint16_t settle_time;
        uint8_t servo_full_angle;
        uint8_t servo_half_angle;
        uint8_t servo_retract_angle;
        uint16_t servo_min_pulse;
        uint16_t servo_max_pulse;
        uint8_t reserved[128];
    } feeder_config_t;

    typedef enum : uint8_t
    {
        /// Feeder has been disabled.
        FEEDER_DISABLED,

        /// Feeder is IDLE.
        FEEDER_IDLE,

        /// Feeder is currently moving.
        FEEDER_MOVING,

        /// Feeder is currently moving.
        FEEDER_RETRACTING
    } feeder_status_t;

    typedef enum : uint8_t
    {
        /// Feeder position is not known.
        POSITION_UNKNOWN,

        /// Feeder position is fully advanced.
        POSITION_ADVANCED_FULL,

        /// Feeder position is half way advanced.
        POSITION_ADVANCED_HALF,

        /// Feeder position is retracted.
        POSITION_RETRACT
    } feeder_position_t;

    /// Feeder index number in relation to all other feeders.
    const std::size_t id_;

    /// Unique ID for this feeder.
    const uint32_t uuid_;

    /// PCA9685 instance to use for this feeder.
    std::shared_ptr<PCA9685> pca9685_;

    /// MCP23017 instance to use for this feeder.
    std::shared_ptr<MCP23017> mcp23017_;

    /// IO Expander channel for this feeder.
    const uint8_t channel_;

    /// Configuration for this feeder.
    feeder_config_t config_;

    /// last known status of this feeder.
    feeder_status_t status_{FEEDER_DISABLED};

    feeder_position_t position_{POSITION_UNKNOWN};
    feeder_position_t target_{POSITION_UNKNOWN};
    int16_t movement_{0};
    asio::system_timer timer_;

    void configure();

    void update(asio::error_code error);
};