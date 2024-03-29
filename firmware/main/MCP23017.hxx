/*
 * SPDX-FileCopyrightText: 2022 Mike Dunston (atanisoft)
 *
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#pragma once

#include "I2Cbus.hxx"
#include <cmath>
#include <esp_err.h>
#include <endian.h>
#include <functional>

/// Manages a single MCP23017 IO Expander IC.
class MCP23017 : public std::enable_shared_from_this<MCP23017>
{

public:
    /// Constructor.
    ///
    /// @param i2c @ref I2C_t instance to use for this @ref MCP23017 instance.
    MCP23017(I2C_t &i2c, asio::io_context &context)
        : i2c_(i2c), timer_(context)
    {
    }

    /// Configures this @ref MCP23017 instance.
    ///
    /// @param address I2C device address to use.
    ///
    /// @return ESP_OK if the device was detected and configured, any other
    /// value indicates failure.
    esp_err_t configure(const uint8_t address)
    {
        addr_ = address;

        esp_err_t res = i2c_.testConnection(addr_);
        if (res != ESP_OK)
        {
            return res;
        }

        // Configure all IO as inputs.
        res = i2c_.writeBytes(addr_, IO_DIR_A, {0xFFFF});
        if (res != ESP_OK)
        {
            return res;
        }

        // Enable pull-ups on all IO pins.
        res = i2c_.writeBytes(addr_, GPIO_PULL_A, {0xFFFF});
        if (res != ESP_OK)
        {
            return res;
        }

        // start background updates
        update({});

        return ESP_OK;
    }

    /// Returns the current state of an IO channel.
    ///
    /// @param channel to return status of.
    ///
    /// @return true if the channel is HIGH, false if the channel is LOW.
    bool state(uint8_t channel)
    {
        // If the channel is 0-7 it uses the first port.
        if (channel < 8)
        {
            return state_[0] & (1 << channel);
        }
        // channel 8-15 use the second port.
        return state_[1] & (1 << (channel - 8));
    }

    /// Subscribes to state change notifications for a single IO pin.
    ///
    /// @param channel to subscribe to.
    /// @param callback Callback to invoke upon state change.
    ///
    /// Signature for @param callback is: void function(bool state).
    void subscribe(uint8_t channel, std::function<void(bool)> callback)
    {
        callbacks_[channel] = std::move(callback);
    }

    uint8_t get_address() const
    {
        return addr_;
    }

    /// maximum number of PWM channels supported by the MCP23017.
    static constexpr size_t NUM_CHANNELS = 16;

private:
    /// Log tag to use for this class.
    static constexpr const char *const TAG = "MCP23017";

    /// Interval at which to poll the current state of the IO pins.
    static constexpr std::size_t POLLING_INTERVAL_MS = 50;

    /// Device register offsets.
    enum REGISTERS
    {
        /// IO Direction A control register address.
        IO_DIR_A = 0x00,

        /// IO Direction B control register address.
        IO_DIR_B = 0x01,

        GPIO_PULL_A = 0x0C,
        GPIO_PULL_B = 0x0D,

        INPUT_A = 0x12,
        INPUT_B = 0x13,

        OUTPUT_A = 0x14,
        OUTPUT_B = 0x15
    };

    /// I2C device address for this device.
    uint8_t addr_;

    /// @ref I2C_t instance to use for this device.
    I2C_t &i2c_;

    /// Last known states of the IO pins.
    uint8_t state_[2];

    /// Collection of callbacks to invoke when state has changed.
    std::function<void(bool)> callbacks_[NUM_CHANNELS];

    /// Background timer instance.
    asio::system_timer timer_;

    /// Background update task which reads the latest state of the IO pins.
    ///
    /// @param error @ref asio::error_code provided by the @ref timer_.
    void update(asio::error_code error)
    {
        if (!error)
        {
            // read the current state of IO pins using a local storage so we
            // can compare it after reading with last reading.
            uint8_t state[2];
            ESP_ERROR_CHECK_WITHOUT_ABORT(
                i2c_.readBytes(addr_, INPUT_A, 2, state));

            // verify if there are any state changes that are interesting to
            // subscribers.
            for (std::size_t index = 0; index < 8; index++)
            {
                // check first eight inputs
                if ((state[0] & (1 << index)) != (state_[0] & (1 << index)))
                {
                    if (callbacks_[index])
                    {
                        callbacks_[index](state[0] & (1 << index));
                    }
                }

                // check second eight inputs
                if ((state[1] & (1 << index)) != (state_[1] & (1 << index)))
                {
                    if (callbacks_[index + 8])
                    {
                        callbacks_[index + 8](state[1] & (1 << index));
                    }
                }
            }
            // stash the newly updated state.
            state_[0] = state[0];
            state_[1] = state[1];

            // reset the timer to call this function again after the timeout.
            timer_.expires_from_now(
                std::chrono::milliseconds(POLLING_INTERVAL_MS));
            timer_.async_wait(std::bind(&MCP23017::update, shared_from_this(),
                                        std::placeholders::_1));
        }
    }
};