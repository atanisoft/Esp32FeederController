/* =========================================================================
I2Cbus library is placed under the MIT License
Copyright 2017 Natanael Josue Rabello. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
 ========================================================================= */

#include "I2Cbus.hxx"
#include <cstdio>
#include <cstdint>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#define SEMAPHORE_TAKE_RECURSIVE() xSemaphoreTakeRecursive(_i2c_mutex, portMAX_DELAY)
#define SEMAPHORE_GIVE_RECURSIVE() xSemaphoreGiveRecursive(_i2c_mutex)

#define I2C_MASTER_ACK_EN true   /*!< Enable ack check for master */
#define I2C_MASTER_ACK_DIS false /*!< Disable ack check for master */

static const char *TAG __attribute__((unused)) = "I2Cbus";

/*******************************************************************************
 * OBJECTS
 ******************************************************************************/
I2C_t i2c0 = i2cbus::I2C(I2C_NUM_0);
I2C_t i2c1 = i2cbus::I2C(I2C_NUM_1);

/* ^^^^^^
 * I2Cbus
 * ^^^^^^ */
namespace i2cbus
{

    /*******************************************************************************
     * SETUP
     ******************************************************************************/
    I2C::I2C(i2c_port_t port) : port{port}, ticksToWait{pdMS_TO_TICKS(kDefaultTimeout)}
    {
    }

    I2C::~I2C()
    {
        close();
    }

    esp_err_t I2C::begin(gpio_num_t sda_io_num, gpio_num_t scl_io_num, uint32_t clk_speed) const
    {
        return begin(sda_io_num, scl_io_num, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE, clk_speed);
    }

    esp_err_t I2C::begin(gpio_num_t sda_io_num, gpio_num_t scl_io_num, gpio_pullup_t sda_pullup_en,
                         gpio_pullup_t scl_pullup_en, uint32_t clk_speed) const
    {
        i2c_config_t conf;
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = sda_io_num;
        conf.sda_pullup_en = sda_pullup_en;
        conf.scl_io_num = scl_io_num;
        conf.scl_pullup_en = scl_pullup_en;
        conf.master.clk_speed = clk_speed;
        conf.clk_flags = 0;
        SEMAPHORE_TAKE_RECURSIVE();
        esp_err_t err = i2c_param_config(port, &conf);
        if (!err)
        {
            err = i2c_driver_install(port, conf.mode, 0, 0, 0);
        }
        SEMAPHORE_GIVE_RECURSIVE();
        return err;
    }

    esp_err_t I2C::close() const
    {
        SEMAPHORE_TAKE_RECURSIVE();
        esp_err_t err = i2c_driver_delete(port);
        SEMAPHORE_GIVE_RECURSIVE();
        return err;
    }

    void I2C::setTimeout(uint32_t ms)
    {
        SEMAPHORE_TAKE_RECURSIVE();
        ticksToWait = pdMS_TO_TICKS(ms);
        SEMAPHORE_GIVE_RECURSIVE();
    }

    /*******************************************************************************
     * WRITING
     ******************************************************************************/
    esp_err_t I2C::writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data, int32_t timeout) const
    {
        uint8_t buffer;
        SEMAPHORE_TAKE_RECURSIVE();
        esp_err_t err = readByte(devAddr, regAddr, &buffer, timeout);
        if (!err)
        {
            buffer = data ? (buffer | (1 << bitNum)) : (buffer & ~(1 << bitNum));
            err = writeByte(devAddr, regAddr, buffer, timeout);
        }
        SEMAPHORE_GIVE_RECURSIVE();
        return err;
    }

    esp_err_t I2C::writeBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data, int32_t timeout) const
    {
        uint8_t buffer;
        SEMAPHORE_TAKE_RECURSIVE();
        esp_err_t err = readByte(devAddr, regAddr, &buffer, timeout);
        if (!err)
        {
            uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
            data <<= (bitStart - length + 1);
            data &= mask;
            buffer &= ~mask;
            buffer |= data;
            err = writeByte(devAddr, regAddr, buffer, timeout);
        }
        SEMAPHORE_GIVE_RECURSIVE();
        return err;
    }

    esp_err_t I2C::writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data, int32_t timeout) const
    {
        return writeBytes(devAddr, regAddr, 1, &data, timeout);
    }

    esp_err_t I2C::writeBytes(uint8_t devAddr, uint8_t regAddr, size_t length, const uint8_t *data, int32_t timeout) const
    {
        SEMAPHORE_TAKE_RECURSIVE();
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
        i2c_master_write_byte(cmd, regAddr, I2C_MASTER_ACK_EN);
        if (length > 0 && data != nullptr)
        {
           i2c_master_write(cmd, (uint8_t *) data, length, I2C_MASTER_ACK_EN);
        }
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
        i2c_cmd_link_delete(cmd);
        SEMAPHORE_GIVE_RECURSIVE();
        if (err)
        {
            ESP_LOGE(TAG, "[port:%d, slave:0x%X] Failed to write %d bytes to__ register 0x%X, error: 0x%X",
                     port, devAddr, length, regAddr, err);
        }
        return err;
    }

    esp_err_t I2C::writeBytes(uint8_t devAddr, uint8_t regAddr, std::vector<uint16_t> values, int32_t timeout) const
    {
        SEMAPHORE_TAKE_RECURSIVE();
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
        i2c_master_write_byte(cmd, regAddr, I2C_MASTER_ACK_EN);

        for (uint16_t value : values)
        {
            i2c_master_write_byte(cmd, (value & 0xff), I2C_MASTER_ACK_DIS);
            i2c_master_write_byte(cmd, (value >> 8) & 0xff, I2C_MASTER_ACK_EN);
        }
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
        i2c_cmd_link_delete(cmd);
        SEMAPHORE_GIVE_RECURSIVE();
        if (err)
        {
            ESP_LOGE(TAG, "[port:%d, slave:0x%X] Failed to write %d bytes to__ register 0x%X, error: 0x%X",
                     port, devAddr, values.size() * 2, regAddr, err);
        }
        return err;
    }

    /*******************************************************************************
     * READING
     ******************************************************************************/
    esp_err_t I2C::readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data, int32_t timeout) const
    {
        return readBits(devAddr, regAddr, bitNum, 1, data, timeout);
    }

    esp_err_t I2C::readBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data, int32_t timeout) const
    {
        uint8_t buffer;
        esp_err_t err = readByte(devAddr, regAddr, &buffer, timeout);
        if (!err)
        {
            uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
            buffer &= mask;
            buffer >>= (bitStart - length + 1);
            *data = buffer;
        }
        return err;
    }

    esp_err_t I2C::readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, int32_t timeout) const
    {
        return readBytes(devAddr, regAddr, 1, data, timeout);
    }

    esp_err_t I2C::readBytes(uint8_t devAddr, uint8_t regAddr, size_t length, uint8_t *data, int32_t timeout) const
    {
        SEMAPHORE_TAKE_RECURSIVE();
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
        i2c_master_write_byte(cmd, regAddr, I2C_MASTER_ACK_EN);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, I2C_MASTER_ACK_EN);
        i2c_master_read(cmd, data, length, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
        i2c_cmd_link_delete(cmd);
        SEMAPHORE_GIVE_RECURSIVE();
        if (err)
        {
            ESP_LOGE(TAG, "[port:%d, slave:0x%X] Failed to read %d bytes from register 0x%X, error: 0x%X",
                     port, devAddr, length, regAddr, err);
        }
        return err;
    }

    /*******************************************************************************
     * UTILS
     ******************************************************************************/
    esp_err_t I2C::testConnection(uint8_t devAddr, int32_t timeout) const
    {
        SEMAPHORE_TAKE_RECURSIVE();
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
        i2c_cmd_link_delete(cmd);
        SEMAPHORE_GIVE_RECURSIVE();
        return err;
    }

    uint8_t I2C::scanner() const
    {
        constexpr int32_t scanTimeout = 20;
        SEMAPHORE_TAKE_RECURSIVE();
        ESP_LOGI(TAG, ">> I2C scanning ...");
        uint8_t count = 0;
        for (size_t i = 0x3; i < 0x78; i++)
        {
            if (testConnection(i, scanTimeout) == ESP_OK)
            {
                ESP_LOGI(TAG, "- Device found at address 0x%X", i);
                count++;
            }
        }
        if (count == 0)
        {
            ESP_LOGI(TAG, "- No I2C devices found!");
        }
        SEMAPHORE_GIVE_RECURSIVE();
        return count;
    }

} // namespace i2cbus
