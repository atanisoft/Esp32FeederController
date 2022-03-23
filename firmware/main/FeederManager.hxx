#pragma once

#include <asio.hpp>
#include <map>

#include "config.hxx"
#include "I2Cbus.hxx"
#include "PCA9685.hxx"
#include "MCP23017.hxx"
#include "GCodeServer.hxx"
#include "Feeder.hxx"

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
    FeederManager(GCodeServer &server, asio::io_context &context);

private:
    /// Log tag to use for this class.
    static constexpr const char *const TAG = "feeder_mgr";

    /// Command ID for moving a feeder.
    static constexpr const char *const FEEDER_MOVE_CMD = "M610";

    /// Command ID for post-pick of a feeder.
    static constexpr const char *const FEEDER_POST_PICK_CMD = "M611";

    /// Command ID for displaying status a feeder (or all feeders).
    static constexpr const char *const FEEDER_STATUS_CMD = "M612";

    /// Command ID for configuring a feeder.
    static constexpr const char *const FEEDER_CONFIGURE_CMD = "M613";

    /// Command ID for enabling a feeder.
    static constexpr const char *const FEEDER_ENABLE_CMD = "M614";

    /// Command ID for disabling a feeder.
    static constexpr const char *const FEEDER_DISABLE_CMD = "M615";

    /// NVS key to use for the @ref FeederManager configuration.
    static constexpr const char *const NVS_FEEDER_MGR_CFG_KEY = "mgr_cfg";

    /// Base address to start from when searching for PCA9685 devices.
    static constexpr uint8_t PCA9685_BASE_ADDRESS = 0x40;

    /// Maximum number of PCA9685 devices to search for.
    static constexpr std::size_t MAX_PCA9685_COUNT = 8;

    /// Base address to start from when searching for MCP23017 devices.
    static constexpr uint8_t MCP23017_BASE_ADDRESS = 0x20;

    /// Maximum number of MCP23017 devices to search for.
    static constexpr std::size_t MAX_MCP23017_COUNT = 8;

    /// Maximum number of feeders to configure.
    static constexpr std::size_t MAX_FEEDER_COUNT =
        MAX_PCA9685_COUNT * PCA9685::NUM_CHANNELS;

    /// Configuration parameters that are persisted.
    typedef struct
    {
        uint32_t feeder_uuid[MAX_FEEDER_COUNT];
    } feeder_manager_config_t;

    /// I2C instance used for managing feeders.
    I2C_t &i2c_;

    /// Collection of PCA9685 devices used by the feeders for servo control.
    std::vector<std::shared_ptr<PCA9685>> pca9685_;

    /// Collection of MCP23017 devices used by the feeders for feedback.
    std::vector<std::shared_ptr<MCP23017>> mcp23017_;

    /// Collection of feeders.
    std::vector<std::shared_ptr<Feeder>> feeders_;

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
    /// Command format: M610 N{feeder}
    GCodeServer::command_return_type feeder_move(GCodeServer::command_args args);

    /// Handles the post-pick action for a feeder (M611).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M611 N{feeder}
    GCodeServer::command_return_type feeder_post_pick(GCodeServer::command_args args);

    /// Handles the status request for a feeder (M612).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M612 N{feeder}
    GCodeServer::command_return_type feeder_status(GCodeServer::command_args args);

    /// Handles the feeder enable request (M614).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M614 N{feeder}
    GCodeServer::command_return_type feeder_enable(GCodeServer::command_args args);

    /// Handles the feeder disable request (M615).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M615 N{feeder}
    GCodeServer::command_return_type feeder_disable(GCodeServer::command_args args);

    /// Handles the configure request for a feeder (M613).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M613 N{feeder} A{advance angle}
    ///                 B{half advance angle} C{retract angle}
    ///                 F{feed lenght} U{settle time} V{min pulse} W{max pulse}
    GCodeServer::command_return_type feeder_configure(GCodeServer::command_args args);
};