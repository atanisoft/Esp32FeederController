/*
 * SPDX-FileCopyrightText: 2022 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#pragma once

#include <asio.hpp>
#include <functional>
#include <map>
#include <memory>

#include "config.hxx"
#include "PCA9685.hxx"
#include "MCP23017.hxx"
#include "GCodeServer.hxx"

/// Class for controlling a single Feeder using a @ref PCA9685 for servo
/// movement and @ref MCP23017 for capturing the status of the tape pulling
/// mechanism.
class Feeder : public std::enable_shared_from_this<Feeder>
{
public:
    /// Constructor.
    ///
    /// @param uuid Unique identifier for this feeder.
    /// @param pca9685 @ref PCA9685 to use for this feeder.
    /// @param channel @ref IO Expander channel assigned to this feeder.
    /// @param context @ref asio::io_context to use for this feeder.
    /// @param mcp23017 @ref MCP23017 to use for this feeder.
    Feeder(std::size_t id, uint32_t uuid, std::shared_ptr<PCA9685> pca9685,
           uint8_t channel, asio::io_context &context,
           std::shared_ptr<MCP23017> mcp23017 = nullptr);

    /// Instructs the feeder to move to a retracted position.
    ///
    /// @return true if command was accepted, false otherwise.
    bool retract();

    /// Instructs the feeder to move forward to the next part.
    ///
    /// @param distance Distance to move the feeder forward, passing zero will
    /// trigger the default movement distance to be used.
    ///
    /// @return true if command was accepted, false otherwise.
    bool move(uint8_t distance = 0);

    /// Instructs the feeder to process any post-pick actions.
    ///
    /// @return true if command was accepted, false otherwise.
    bool post_pick();

    /// Converts the state of this feeder to a string which can be sent to a
    /// connected client.
    ///
    /// @return @ref std::string containing the current state and configuration
    /// of this feeder.
    std::string status();

    /// Enables this feeder for use.
    ///
    /// @return true if command was accepted, false otherwise.
    bool enable();

    /// Disables this feeder for use.
    ///
    /// @return true if command was accepted, false otherwise.
    bool disable();

    /// Configures this feeder.
    ///
    /// @param advance_angle Servo angle to move to for full advancement
    /// distance.
    /// @param half_advance_angle Servo angle to move to for half advancement
    /// distance.
    /// @param retract_angle Servo angle to move to for retraction.
    /// @param feed_length Length to move the tape forward to reach the next
    /// part, must be a multiple of 2mm.
    /// @param settle_time_ms Time to allow the servo to settle before moving
    /// again.
    /// @param min_pulse Minimum number of pulses to send.
    /// @param max_pulse Maximum number of pulses to send.
    /// @param ignore_feedback When set to zero the feedback sensor will not
    /// be checked, when set to 1 (or higher) the feedback sensor will be
    /// checked.
    /// @param movement_speed_ms Number of milliseconds to delay between servo
    /// movements, when set to zero movement is immediate.
    /// @param movement_degrees Maximum number of degrees to move the servo arm
    /// in a single movement.
    void configure(uint8_t advance_angle, uint8_t half_advance_angle,
                   uint8_t retract_angle, uint8_t feed_length,
                   uint16_t settle_time_ms, uint8_t min_pulse,
                   uint8_t max_pulse, int8_t ignore_feedback,
                   int16_t movement_speed_ms, uint8_t movement_degrees);

    /// @return true if the feeder is processing a previous action, false
    /// otherwise.
    bool is_busy();

    /// @return true if the feeder has been enabled, false otherwise.
    bool is_enabled();

    /// @return true if the feeder is currently moving, false otherwise.
    bool is_moving();

    /// Checks the feedback input from the feeder.
    ///
    /// @return true if the feedback sensor indicates the tape is sufficiently
    /// tensioned, false otherwise. If the feedback sensor is not in use this
    /// will always return true.
    bool is_tensioned();

    /// Callback for the @ref MCP23017 to call when the configured feedback pin
    /// changes state.
    ///
    /// @param state State of the IO pin.
    void feedback_state_changed(bool state);

    /// Loads configuration for the feeder and starts listening for feedback, if
    /// enabled.
    void initialize();

private:
    /// Log tag to use for this class.
    static constexpr const char *const TAG = "feeder";

    /// Movement defined by mechanicial limitations.
    static constexpr uint8_t FEEDER_MECHANICAL_ADVANCE_LENGTH = 4;

    /// Persistent Feeder configuration.
    typedef struct
    {
        /// Number of millimeters to move the tape forward to reach the next
        /// part. Must be a multiple of 2mm.
        uint8_t feed_length;

        /// Number of milliseconds to allow for the servo movement to complete.
        uint16_t settle_time_ms;

        /// Angle to move the servo to for fully advanced position.
        uint8_t servo_full_angle;

        /// Angle to move the servo to for half advanced position.
        uint8_t servo_half_angle;

        /// Angle to move the servo to for retraction prior to further movement.
        uint8_t servo_retract_angle;

        /// Minimum number of pulses to send the servo as part of movement.
        uint16_t servo_min_pulse;

        /// Maximum number of pulses to send the servo as part of movement.
        uint16_t servo_max_pulse;

        /// When set to non-zero the feedback sensor will be ignored.
        uint8_t ignore_feedback;

        /// When set to non-zero the servo will move at most this number of
        /// degrees in a single movement.
        uint8_t movement_degrees;

        /// Number of milliseconds to delay between servo movements.
        uint16_t movement_interval_ms;

        /// Reserved space for expansion.
        uint8_t reserved[124];
    } feeder_config_t;

    /// Feeder status definitions.
    typedef enum : uint8_t
    {
        /// Feeder has been disabled.
        FEEDER_DISABLED,

        /// Feeder is IDLE.
        FEEDER_IDLE,

        /// Feeder is currently moving.
        FEEDER_MOVING
    } feeder_status_t;

    /// Feeder position definitions.
    typedef enum : uint8_t
    {
        /// Feeder position is not known.
        POSITION_UNKNOWN,

        /// Feeder position is fully advanced.
        POSITION_ADVANCED_FULL,

        /// Feeder position is half way advanced.
        POSITION_ADVANCED_HALF,

        /// Feeder position is retracted.
        POSITION_RETRACTED
    } feeder_position_t;

    /// Size of the persistent configuration data.
    static constexpr std::size_t configsize_ = sizeof(feeder_config_t);

    /// Feeder index number in relation to all other feeders.
    const std::size_t id_;

    /// Unique ID for this feeder.
    const uint32_t uuid_;

    /// Unique NVS key used for persistent configuration storage.
    std::string nvskey_;

    /// PCA9685 instance to use for this feeder.
    std::shared_ptr<PCA9685> pca9685_;

    /// MCP23017 instance to use for this feeder.
    std::shared_ptr<MCP23017> mcp23017_;

    /// IO Expander channel for this feeder.
    const uint8_t channel_;

    /// Last known state of the feeder tension feedback line.
    bool tensioned_{true};

    /// Tracking holder for manual advancement by pressing/releasing the tape
    /// tension arm.
    bool advance_{false};

    /// Configuration for this feeder.
    feeder_config_t config_;

    /// Last known status of this feeder.
    feeder_status_t status_{FEEDER_DISABLED};

    /// Last known position for this feeder.
    feeder_position_t position_{POSITION_UNKNOWN};
    
    /// Target to move the servo arm to.
    uint8_t targetDegrees_{0};

    /// Last known position of the servo arm.
    uint8_t currentDegrees_{0};

    /// Remaining movement (if any) for the feeder after it completes the
    /// current movement action.
    std::size_t movement_{0};

    /// Background timer which invokes @ref update(asio::error_code) based on
    /// the configured settle time.
    asio::system_timer timer_;

    /// Mutex protecting configuration, status, etc.
    std::mutex mux_;

    /// Callback from @ref timer_ used to handle post-movement actions such as
    /// continuing movement or turning off the servo.
    ///
    /// @param error @ref asio::error_code containing the error status from the
    /// @ref timer_.
    void update(asio::error_code error);

    /// Callback from @ref timer_ used to continue movement of the servo arm to
    /// the target position.
    ///
    /// @param error @ref asio::error_code containing the error status from the
    /// @ref timer_.
    void servo_movement_complete(asio::error_code error);

    /// Moves the servo to the retracted position.
    void retract_locked();

    /// Moves the servo as required to complete movement actions.
    void move_locked();

    /// Moves the servo to a specific angle and starts @ref timer_ to complete
    /// any pending movement actions.
    void set_servo_angle(uint8_t angle);
};