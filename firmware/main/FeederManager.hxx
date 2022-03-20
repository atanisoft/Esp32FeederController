#pragma once

#include <asio.hpp>
#include <map>

#include "config.hxx"
#include "I2Cbus.hxx"
#include "PCA9685.hxx"
#include "GCodeServer.hxx"

/// Manages all connected Feeders.
///
/// Feeders are registered into uniquely identified banks using a zero
/// based index as the bank-id. A bank contains up to three groups of 16
/// feeders and can contain up to 48 feeders total.
///
/// The maximum number of banks can be defined in config.hxx.
class FeederManager
{
public:
    /// Constructor.
    ///
    /// @param server @ref GCodeServer to register feeder commands with.
    FeederManager(GCodeServer &server);

private:
    /// Maximum number of feeders to configure per bank.
    static constexpr std::size_t MAX_FEEDERS_PER_BANK = 32;

    /// Maximum number of feeder groups.
    static constexpr std::size_t MAX_FEEDER_GROUPS = 10;

    /// Log tag to use for this class.
    static constexpr const char *const TAG = "feeder_mgr";

    /// Command ID for moving a feeder.
    static constexpr const char *const FEEDER_MOVE_CMD = "M610";

    /// Command ID for post-pick of a feeder.
    static constexpr const char *const FEEDER_POST_PICK_CMD = "M611";

    /// Command ID for displaying status a feeder (or all feeders).
    static constexpr const char *const FEEDER_STATUS_CMD = "M612";

    /// Command ID for configuring a feeder group.
    static constexpr const char *const FEEDER_GROUP_CONFIGURE_CMD = "M613";

    /// Command ID for configuring a feeder.
    static constexpr const char *const FEEDER_CONFIGURE_CMD = "M614";

    /// NVS namespace to use for all feeder related configuration.
    static constexpr const char *const NVS_FEEDER_NAMESPACE = "esp32feeder";

    /// NVS key to use for the @ref FeederManager configuration.
    static constexpr const char *const NVS_FEEDER_MGR_CFG_KEY = "mgr_cfg";

    class FeederBank
    {
    public:
        /// Constructor.
        ///
        /// @param i2c @ref I2C_t instance to use for @ref PCA9685.
        /// @param bank Bank identifier for this feeder bank.
        /// @param id Unique identifier for this feeder bank.
        FeederBank(I2C_t &i2c, uint8_t bank, uint32_t id);

        /// Loads configuration from non-volatile storage.
        ///
        /// @return true if the configuration was loaded successfully and the
        /// PCA9685 devices were detected, false otherwise.
        ///
        /// If configuration settings are not present or are corrupted, new
        /// default configuration settings will be generated.
        bool load_configuration();

        GCodeServer::command_return_type retract(uint8_t feeder);

        GCodeServer::command_return_type move(uint8_t feeder);

        GCodeServer::command_return_type post_pick(uint8_t feeder);

        GCodeServer::command_return_type status(uint8_t feeder);

        GCodeServer::command_return_type configure(uint8_t feeder,
                                                   uint8_t advance_angle,
                                                   uint8_t half_advance_angle,
                                                   uint8_t retract_angle,
                                                   uint8_t feed_length,
                                                   uint8_t settle_time,
                                                   uint8_t min_pulse,
                                                   uint8_t max_pulse);

        GCodeServer::command_return_type configure(uint8_t pca9685_a,
                                                   uint8_t pca9685_b,
                                                   uint32_t pca9685_a_freq,
                                                   uint32_t pca9685_b_freq);

    private:
        /// Log tag to use for this class.
        static constexpr const char *const TAG = "feeder_bank";

        /// Number of PCA9685 devices to use for this feeder bank.
        static constexpr std::size_t PCA965_COUNT = 2;

        /// Number of feedback devices to use for this feeder bank.
        static constexpr std::size_t FEEDBACK_COUNT = 2;

        /// Movement defined by mechanicial limitations.
        static constexpr uint8_t FEEDER_MECHANICAL_ADVANCE_LENGTH = 4;

        /// Default fully extended angle in degrees.
        static constexpr uint8_t DEFAULT_FULL_ANGLE = 90;

        /// Default fully extended angle in degrees.
        static constexpr uint8_t DEFAULT_HALF_ANGLE = DEFAULT_FULL_ANGLE / 2;

        /// Default fully extended angle in degrees.
        static constexpr uint8_t DEFAULT_RETRACT_ANGLE = 15;

        /// Default settlement time in milliseconds.
        static constexpr uint16_t DEFAULT_SETTLE_TIME_MS = 240;

        /// Default minimum number of pulses to send the servo.
        static constexpr uint16_t DEFAULT_MIN_PULSE_COUNT = 150;

        /// Default maximum number of pulses to send the servo.
        static constexpr uint16_t DEFAULT_MAX_PULSE_COUNT = 600;

        typedef struct
        {
            uint8_t pca9685_address[PCA965_COUNT];
            uint32_t pca9685_frequency[PCA965_COUNT];
            uint8_t feedback_address[FEEDBACK_COUNT];
            struct
            {
                uint8_t full_angle;
                uint8_t half_angle;
                uint8_t retract_angle;
                uint8_t feed_length;
                uint16_t settle_time;
                uint16_t min_pulse;
                uint16_t max_pulse;
            } feeder_config[MAX_FEEDERS_PER_BANK];
            uint8_t reserved[128];
        } feeder_bank_config_t;

        /// Bank identifier for this bank.
        const uint8_t bank_;

        /// Unique ID for this bank.
        const uint32_t id_;

        /// Flag indicating that all required hardware has been found and is
        /// ready to use.
        bool ready_{false};

        /// PCA9685 instances to use for this bank.
        PCA9685 pca9685_[PCA965_COUNT];

        /// Configuration for this bank.
        feeder_bank_config_t config_;

        /// Status of each feeder in this bank.

        enum feeder_status_t : uint8_t
        {
            /// Feeder has been disabled.
            FEEDER_DISABLED,

            /// Feeder is IDLE.
            FEEDER_IDLE,

            /// Feeder is currently moving.
            FEEDER_MOVING,

            /// Feeder is currently moving.
            FEEDER_RETRACTING
        } status_[MAX_FEEDERS_PER_BANK];

        enum feeder_position_t : uint8_t
        {
            /// Feeder position is not known.
            POSITION_UNKNOWN,

            /// Feeder position is fully advanced.
            POSITION_ADVANCED_FULL,

            /// Feeder position is half way advanced.
            POSITION_ADVANCED_HALF,

            /// Feeder position is retracted.
            POSITION_RETRACT
        } position_[MAX_FEEDERS_PER_BANK],
            target_[MAX_FEEDERS_PER_BANK];

        int16_t movement_[MAX_FEEDERS_PER_BANK];
    };

    /// Configuration parameters that are persisted.
    typedef struct
    {
        uint8_t bank_count;
        uint32_t feeder_group_id[MAX_FEEDER_GROUPS];
        uint8_t reserved[256];
    } feeder_manager_config_t;

    /// I2C instance used for managing feeders.
    I2C_t &i2c_;

    /// Collection of servo drivers, grouped by bank.
    std::vector<std::unique_ptr<FeederBank>> banks_;

    /// Utility function to extract a parameter from the provided arguments.
    ///
    /// @param arg Argument to search for.
    /// @param args Arguments to search through.
    /// @param value Variable to capture the parameter value.
    ///
    /// @return true upon successful extraction of parameter, false otherwise.
    ///
    /// NOTE: @param arg can be more than one character but typically will be
    /// a single character string.
    template <typename T>
    bool extract_arg(std::string arg, GCodeServer::command_args args,
                     T &value);

    /// Handles the request to move a feeder (M610).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M610 T{bank} N{feeder}
    GCodeServer::command_return_type feeder_move(GCodeServer::command_args args);

    /// Handles the post-pick action for a feeder (M611).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M611 T{bank} N{feeder}
    GCodeServer::command_return_type feeder_post_pick(GCodeServer::command_args args);

    /// Handles the status request for a feeder (M612).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M612 T{bank} N{feeder}
    GCodeServer::command_return_type feeder_status(GCodeServer::command_args args);

    /// Handles the configur request for a feeder (M613).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M613 T{bank} A{pca9865 #1 address} B{pca9865 #2 address}
    ///                 C{pca9865 #1 PWM frequency} D{pca9865 #2 PWM frequency}
    GCodeServer::command_return_type feeder_group_configure(GCodeServer::command_args args);

    /// Handles the configur request for a feeder (M614).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M614 T{bank} N{feeder} A{advance angle}
    ///                 B{half advance angle} C{retract angle}
    ///                 F{feed lenght} U{settle time} V{min pulse} W{max pulse}
    GCodeServer::command_return_type feeder_configure(GCodeServer::command_args args);
};