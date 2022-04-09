# Esp32FeederController Firmware

The Esp32FeederController is built using two PCA9685 PWM generators and two
MCP23017 IO Expander ICs. The [custom PCB](../pcb/FeederController/) allows
control of up to 32 feeders with a single PCB.

## Configuring your build environment

Esp32FeederController is built using [ESP-IDF](https://github.com/espressif/esp-idf),
most testing is done with the master branch (v5.0 or later) but earlier stable
versions should work as well. Please follow [these](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation)
instructions for setting up your build environment.

## Building and flashing the firmware

Once the environment has been setup you are almost ready to build and upload the
binary to the ESP32. Before building you will need to configure your WiFi access
details in `firmware/main/config.hxx` in the `WIFI_SSID` and `WIFI_PASSWORD`
fields. Once the two WiFi fields have been updated you are ready to build.

Plug in the ESP32 to your computer and execute the following commands to build
and flash the firmware to the ESP32:

```
idf.py build flash monitor -p /dev/ttyUSB0
```

Adjust `/dev/ttyUSB0` to match the port assigned to the ESP32 when it is plugged
into your computer.

Once the firmware has been uploaded the ESP32 will restart and the serial output
will be displayed in the terminal. The startup process should take 5-10 seconds
the first time and will display the assigned IP address:

```
I (587) main: Initializing NVS
I (650) wifi_mgr: Setting hostname to "esp32feeder".
I (656) wifi_mgr: Configuring Station (SSID:YourWiFiNetwork)
I (657) wifi_mgr: Starting WiFi stack
I (756) wifi_mgr: WiFi station started.
I (1935) wifi_mgr: Connected to SSID:YourWiFiNetwork
I (1936) wifi_mgr: [1/36] Waiting for IP address assignment.
I (2104) wifi_mgr: IP address:10.0.0.13
I (2105) wifi_mgr: [2/36] Waiting for IP address assignment.
I (2114) gcode_server: Waiting for connections on 10.0.0.13:8989...
```
Make note of the IP address that is reported as you will need it for OpenPnP
configuration.
