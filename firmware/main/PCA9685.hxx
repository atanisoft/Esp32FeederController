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

/// Manages a single PCA9685 PWM controller IC.
class PCA9685
{

public:
    /// Constructor.
    ///
    /// @param i2c @ref I2C_t instance to use for this @ref PCA9685 instance.
    PCA9685(I2C_t &i2c) : i2c_(i2c)
    {
    }

    /// Configures this @ref PCA9685 instance.
    ///
    /// @param address I2C device address to use.
    /// @param frequency PWM Frequency to use, default is 50Hz.
    ///
    /// @return ESP_OK if the device was detected and configured, any other
    /// value indicates failure.
    esp_err_t configure(const uint8_t address,
                        const uint32_t frequency = 50)
    {
        addr_ = address;

        esp_err_t res = i2c_.testConnection(addr_);
        if (res != ESP_OK)
        {
            return res;
        }

        // ensure the PWM frequency is within normal range.
        if (frequency > (INTERNAL_CLOCK_FREQUENCY / (4096 * 4)))
        {
            ESP_LOGE(TAG, "[%02x] Invalid PWM frequency provided: %" PRIu32,
                     address, frequency);
            return ESP_ERR_INVALID_ARG;
        }

        MODE1_REGISTER mode1;
        mode1.auto_increment = 1;
        mode1.sleep = 1;
        mode1.all_call = 0;
        ESP_LOGD(TAG, "[%02x] Configuring MODE1 register: %02x", address,
                 mode1.value);
        res = i2c_.writeByte(addr_, REGISTERS::MODE1, mode1.value);
        if (res != ESP_OK)
        {
            return res;
        }

        uint8_t prescaler =
            (INTERNAL_CLOCK_FREQUENCY / (4096 * frequency)) - 1;
        ESP_LOGD(TAG, "[%02x] Configuring pre-scaler register: %d", address,
                 prescaler);
        res = i2c_.writeByte(addr_, REGISTERS::PRE_SCALE, prescaler);
        if (res != ESP_OK)
        {
            return res;
        }

        /* if using internal clock */
        mode1.sleep = 0;
        res = i2c_.writeByte(addr_, REGISTERS::MODE1, mode1.value);
        if (res != ESP_OK)
        {
            return res;
        }

        MODE2_REGISTER mode2;
        mode2.output_check = 1;
        return i2c_.writeByte(addr_, REGISTERS::MODE2, mode2.value);
    }

    /// Configures one PWM output.
    ///
    /// @param channel PWM output to configure.
    /// @param count Number of pulses to generate.
    ///
    /// @return ESP_OK if the PWM output was updated, ESP_ERR_INVALID_ARG if
    /// the @param channel is outside the supported range, all other values
    /// indicate an I2C failure.
    ///
    /// NOTE: passing -1 for @param count will enable the maximum output
    /// frequency. Passing 0 (zero) for count will disable the PWM output
    /// signal.
    esp_err_t set_pwm(const uint8_t channel, const uint16_t count)
    {
        if (channel >= NUM_CHANNELS)
        {
            return ESP_ERR_INVALID_ARG;
        }

        OUTPUT_STATE_REGISTER reg_value;
        if (count >= MAX_PWM_COUNTS)
        {
            reg_value.on.full_on = 1;
            reg_value.off.full_off = 0;
        }
        else if (count == 0)
        {
            reg_value.on.full_on = 0;
            reg_value.off.full_off = 1;
        }
        else
        {
            // the "256" count offset is to help average the current accross
            // all 16 channels when the duty cycle is low.
            reg_value.on.counts = (channel * 256);
            reg_value.off.counts = (count + (channel * 256)) % 0x1000;
        }
        uint8_t output_register = REGISTERS::LED0_ON_L + (channel << 2);
        ESP_LOGV(TAG, "[%02x:%d] Setting PWM to %d:%d", addr_, channel,
                 reg_value.on.value, reg_value.off.value);
        return i2c_.writeBytes(addr_, output_register,
                               {htole16(reg_value.on.value),
                                htole16(reg_value.off.value)});
    }

    /// Utility method to turn off the PWM signal for a single channel.
    ///
    /// @param channel PWM output to turn off.
    ///
    /// @return ESP_OK if the PWM output was updated, ESP_ERR_INVALID_ARG if
    /// the @param channel is outside the supported range, all other values
    /// indicate an I2C failure.
    esp_err_t off(const uint8_t channel)
    {
        return set_pwm(channel, MAX_PWM_COUNTS);
    }

    /// Configures a PWM output to drive a servo to specific angle.
    ///
    /// @param channel PWM output to configure.
    /// @param angle Desired angle to move to.
    /// @param min_pulse_count Minimum pulse count to send to the servo.
    /// @param max_pulse_count Maximum pulse count to send to the servo.
    /// @param min_servo_angle Minimum angle that the servo supports,
    /// default 0 (zero).
    /// @param max_servo_angle Maximum angle that the servo supports,
    /// default 180.
    ///
    /// @return ESP_OK if the PWM output was updated, ESP_ERR_INVALID_ARG if
    /// the @param channel is outside the supported range, all other values
    /// indicate an I2C failure.
    esp_err_t set_servo_angle(const uint8_t channel, const uint16_t angle,
                              const uint16_t min_pulse_count = 150,
                              const uint16_t max_pulse_count = 600,
                              const uint16_t min_servo_angle = 0,
                              const uint16_t max_servo_angle = 180)
    {
        ESP_LOGI(TAG, "[%02x:%d] Moving to %" PRIu16 " deg", addr_, channel, angle);
        const uint16_t pulse_count_range = max_pulse_count - min_pulse_count;
        const uint16_t target_angle =
            std::max(std::min(angle, max_servo_angle), min_servo_angle);
        const uint16_t pulse_count =
            (pulse_count_range * target_angle) / max_servo_angle +
            min_pulse_count;
        return set_pwm(channel, pulse_count);
    }

    uint8_t get_address() const
    {
        return addr_;
    }

    /// maximum number of PWM channels supported by the PCA9685.
    static constexpr size_t NUM_CHANNELS = 16;

    /// maximum number of PWM counts supported by the PCA9685.
    static constexpr size_t MAX_PWM_COUNTS = 4096;

private:
    /// Log tag to use for this class.
    static constexpr const char *const TAG = "PCA9685";

    /// Default internal clock frequency, 25MHz.
    static constexpr uint32_t INTERNAL_CLOCK_FREQUENCY = 25000000;

    /// Device register offsets.
    enum REGISTERS
    {
        /// MODE1 register address.
        MODE1 = 0x00,

        /// MODE2 register address.
        MODE2 = 0x01,

        /// OUTPUT 0 first register address. This is used as a starting offset
        /// for all other output registers.
        LED0_ON_L = 0x6,

        /// Register address used to turn off all outputs.
        ALL_OFF = 0xFC,

        /// Clock pre-scaler divider register address.
        PRE_SCALE = 0xFE,
    };

    /// Mode1 register layout.
    union MODE1_REGISTER
    {
        /// Constructor
        MODE1_REGISTER() : value(0x01)
        {
        }
        /// Full byte value for the register
        uint8_t value;
        struct
        {
            uint8_t all_call        : 1;
            uint8_t sub_addr_3      : 1;
            uint8_t sub_addr_2      : 1;
            uint8_t sub_addr_1      : 1;
            uint8_t sleep           : 1;
            uint8_t auto_increment  : 1;
            uint8_t external_clock  : 1;
            uint8_t restart         : 1;
        };
    };

    /// Mode2 register layout.
    union MODE2_REGISTER
    {
        /// Constructor.
        MODE2_REGISTER() : value(0x04)
        {
        }

        /// Full byte value for the register
        uint8_t value;
        struct
        {
            uint8_t output_enable   : 2;
            uint8_t output_mode     : 1; // 1 = push/pull, 0 = open drain.
            uint8_t output_check    : 1; // 1 = update on ack, 0 = update on stop.
            uint8_t output_inverted : 1;
            uint8_t unused          : 3;
        };
    };

    /// Output channel register layout.
    struct OUTPUT_STATE_REGISTER
    {
        union On
        {
            /// Constructor.
            On() : value(0x0000)
            {
            }

            /// ON Register value
            uint16_t value;
            struct
            {
                /// Number of ON counts.
                uint16_t counts : 12;

                /// Set to full ON.
                uint16_t full_on : 1;

                /// Unused bits.
                uint16_t unused : 3;
            };
        };

        union Off
        {
            /// Constructor.
            Off() : value(0x0000)
            {
            }

            /// OFF Register value
            uint16_t value;
            struct
            {
                /// Number of OFF counts.
                uint16_t counts : 12;

                /// Set to full OF.
                uint16_t full_off : 1;

                /// Unused bits.
                uint16_t unused : 3;
            };
        };

        /// On register instance.
        On on;

        /// Off register instance.
        Off off;
    };

    /// I2C device address for this device.
    uint8_t addr_;

    /// @ref I2C_t instance to use for this device.
    I2C_t &i2c_;
};