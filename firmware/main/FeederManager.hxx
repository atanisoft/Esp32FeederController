#pragma once

#include "config.hxx"

class GCodeServer;

/// Manages all connected Feeders.
///
/// Feeders are registered into uniquely identified banks using a zero
/// based index as the bank-id. A bank contains up to three groups of 16
/// feeders and can contain up to 48 feeders total.
///
/// The maximum number of banks can be defined in config.hxx.
class FeederManager
{
    using cmd_reply = std::pair<bool, std::string>;
    using cmd_args = std::vector<std::string> const &;
public:
    /// Constructor.
    FeederManager();

    /// Configures and starts the @ref FeederManager.
    ///
    /// @param server @ref GCodeServer to register feeder related GCode
    /// handlers with.
    void start(GCodeServer &server);

private:
    /// Maximum number of feeders to configure per bank.
    static constexpr size_t MAX_FEEDERS_PER_BANK = 48;

    /// Total number of feeders to configure.
    static constexpr size_t TOTAL_FEEDER_COUNT = MAX_FEEDERS_PER_BANK * FEEDER_BANK_COUNT;

    /// Log tag to use for this class.
    static constexpr const char *TAG = "feeder_mgr";

    /// Command ID for moving a feeder.
    static constexpr const char *FEEDER_MOVE_CMD = "M610";

    /// Command ID for post-pick of a feeder.
    static constexpr const char *FEEDER_POST_PICK_CMD = "M611";

    /// Command ID for displaying status a feeder (or all feeders).
    static constexpr const char *FEEDER_STATUS_CMD = "M612";

    /// Command ID for configuring a feeder.
    static constexpr const char *FEEDER_CONFIGURE_CMD = "M613";

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
    bool extract_arg(std::string arg, cmd_args args, T &value);

    /// Handles the request to move a feeder (M610).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M610 T{bank} N{feeder}
    cmd_reply feeder_move(cmd_args args);

    /// Handles the post-pick action for a feeder (M611).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M611 T{bank} N{feeder}
    cmd_reply feeder_post_pick(cmd_args args);

    /// Handles the status request for a feeder (M612).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M612 T{bank} N{feeder}
    cmd_reply feeder_status(cmd_args args);

    /// Handles the configur request for a feeder (M613).
    ///
    /// @param args Arguments to the command.
    /// @return status of the request.
    ///
    /// Command format: M613 T{bank} N{feeder} A{advance angle}
    ///                 B{half advance angle} C{retract angle}
    ///                 F{feed lenght} U{settle time} V{min pulse} W{max pulse}
    cmd_reply feeder_configure(cmd_args args);
};