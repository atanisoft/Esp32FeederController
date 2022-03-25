# Esp32FeederController

The Esp32FeederController project allows controlling of multiple 0816 Feeders
using four wire cables.

## Supported Feeders

At this time the following feeder designs should work:

* [Original 0816 Feeder](https://docs.mgrl.de/maschine:pickandplace:feeder:0816feeder).
* [0816 Lumen Feeder](https://github.com/SupaCoder/0816-Lumen-SMT-Feeder-Remix).

### Feeder modifications / Addons

* [Reel holder](https://www.thingiverse.com/thing:3810696)
* [Vertical tape puller](https://github.com/SebG3D/TapePuller/wiki)

## Non-automatic feeders

* [Lumen Push/Pull Feeder](https://github.com/GatCode/LumenPPF)

# Configuring and building the Esp32FeederController

Esp32FeederController is built using [ESP-IDF](https://github.com/espressif/esp-idf),
most testing is done with the master branch (v5.0 or later) but earlier stable
versions should work as well. Please follow [these](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation)
instructions for setting up your build environment.

Once the environment has been setup you are almost ready to build and upload the
binary to the ESP32. Before building you will need to configure your WiFi access
details in `firmware/main/config.hxx` in the `WIFI_SSID` and `WIFI_PASSWORD`
fields. Once the two WiFi fields have been updated you are ready to build.

Plug in the ESP32 to your computer and execute the following commands to build
and flash the firmware to the ESP32:

```
cd firmware
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

# Adding Esp32FeederController to OpenPnP

Adding Esp32FeederController to OpenPnP consists of three steps:

* Create [GcodeDriver](https://github.com/openpnp/openpnp/wiki/GcodeDriver).
* Create [Actuators](https://github.com/openpnp/openpnp/wiki/Setup-and-Calibration%3A-Actuators) for Feeder actions.
* Create [Feeders](https://github.com/openpnp/openpnp/wiki/Setup-and-Calibration%3A-Feeders). 

### Creating the Gcode Driver

Each Esp32FeederController PCB should have it's own [GcodeDriver](https://github.com/openpnp/openpnp/wiki/GcodeDriver).

Steps to add a new [GcodeDriver](https://github.com/openpnp/openpnp/wiki/GcodeDriver) for Esp32FeederController:

* Navigate to the `Machine Setup` tab
* Traverse the list to the `Drivers` section, select and expand it.
* Click the `+` (add) icon and select `GcodeDriver`.
* Select the newly created `GcodeDriver GcodeDriver` entry.
* Set the `Name` so it can be easily identified later.
* Set `Communications Type` to `tcp`.
* Scroll down to the `TCP` section.
* Set `IP Address` based on the reported IP address in the serial output.
* Set `Port` to `8989`.
* Click the `Apply` button to save the updates.
* In the `File` menu select `Save Configuration` to ensure settings are not lost when restarting OpenPnP.

### Creating Actuators

Each Esp32FeederController PCB requires two `Actuators`:

* Feeder Advancement
* Feeder Post Pick

The following steps should be executed for the two `Actuators`:

* Navigate to the `Machine Setup` tab
* Traverse the list to the `Actuators` section, select and expand it.
* Click the `+` (add) icon and select `ReferenceActuator`.
* Select the newly created `ReferenceActuator ReferenceActuator` entry.
* Set `Driver` to the Esp32FeederController driver created previously.
* Set the `Name` so it can be easily identified later.
* Set `Value Type` to `Double`.
* Click the `Apply` button to save the updates.
* In the `File` menu select `Save Configuration` to ensure settings are not lost when restarting OpenPnP.

After the two `Actuators` have been defined it is necessary to configure the
Gcode commands to send for actuation, use the following steps to configure this:

* Navigate to the `Machine Setup` tab
* Traverse the list to the `Drivers` section, select and expand it.
* Click the `GcodeDriver Esp32FeederController` (or whatever name was provided previously).
* Navigate to the `Gcode` tab.
* In `Head Mountable` drop down select `Default`.
  * In `Setting` select `COMMAND_CONFIRM_REGEX` and enter `^ok.*`, be sure to click `Apply` before moving further.
  * In `Setting` select `COMMAND_ERROR_REGEX` and enter `^error.*`, be sure to click `Apply` before moving further.
* In `Head Mountable` drop down select `Actuator: [No Head] Feeder Advancement`.
  * In `Setting` select `ACTUATE_DOUBLE_COMMAND` and enter `M610 N{IntegerValue}*`, be sure to click `Apply` before moving further.
* In `Head Mountable` drop down select `Actuator: [No Head] Feeder Post Pick`.
  * In `Setting` select `ACTUATE_DOUBLE_COMMAND` and enter `M611 N{IntegerValue}*`, be sure to click `Apply` before moving further.
* In the `File` menu select `Save Configuration` to ensure settings are not lost when restarting OpenPnP.

## Configuring Feeders in OpenPnP

There are two types of feeders that will work well with Esp32FeederController:

* [ReferenceAutoFeeder](https://github.com/openpnp/openpnp/wiki/ReferenceAutoFeeder)
* [ReferenceSlotAutoFeeder](https://github.com/openpnp/openpnp/wiki/ReferenceSlotAutoFeeder)

Both operate similarly using the `Actuators` defined above.

* Navigate to the `Feeders` tab
* Click the `+` (add) icon and select `ReferenceAutoFeeder` or `ReferenceSlotAutoFeeder`.
* Select the newly created `ReferenceAutoFeeder ReferenceAutoFeeder` or `ReferenceSlotAutoFeeder ReferenceSlotAutoFeeder` entry.
* Double click on the newly created row in the `Name` column and rename it for easier identification.
* Set `Feed Actuator` to `Feeder Advancement` (or whichever name you provide the `Actuator`)
* Set `Post Pick Actuator` to `Feeder Post Pick` (or whichever name you provide the `Actuator`)
* Set `Actuator Value` on both `Actuators` to the feeder number on the Esp32FeederController PCB minus one (zero indexed).
* Click the `Apply` button to save the updates.
* In the `File` menu select `Save Configuration` to ensure settings are not lost when restarting OpenPnP.

Use the `Test Feed` and `Test post pick` to confirm the feeder is responding correctly.

## Supported Gcode commands

The Esp32FeederController supports multiple Gcode commands, many of which are
not directly used by OpenPnP. Many of the commands are intended to be used with
substitution of values within `{ }` marks, anything within `[ ]` are optional
arguments.

### Feeder Movement (M610)

`M610 N{feeder} [D{distance}]`

* `{feeder}` is the Feeder to be moved forward.
* `{distance}` is the distance to move the feeder and is optional.

### Feeder Post Pick (M611)

`M611 N{feeder}`

* `{feeder}` is the Feeder to be moved as part of post-pick.

### Feeder Status (M612)

`M612 N{feeder}`

* `{feeder}` is the Feeder to retrieve status for.

### Feeder Configuration (M613)

`M613 N{feeder} [A{advance angle}] [B{half advance angle}] [C{retract angle}] [F{feed length}] [U{settle time}] [V{min pulse}] [W{max pulse}] [Z{feedback enabled}]`

All parameters except `{feeder}` are optional.

* `{feeder}` is the Feeder to be configured.
* `{advance angle}` is the angle to move the servo to for full extension.
* `{half advance angle}` is the angle to move the servo to for half extension.
* `{retract angle}` is the angle to move the servo to for retraction.
* `{feed length}` is the number of millimeters (pitch) to move the feeder forward when moving to the next part and must be a multiple of 2.
* `{settle time}` is the number of milliseconds to delay between servo movements.
* `{min pulse}` is the minimum number of pulses to send the servo.
* `{max pulse}` is the maximum number of pulses to send the servo.
* `{feedback enabled}` is used to enable or disable feedback checking as part of movement, set to zero to disable or one to enable.

### Enable Feeder (M614)

`M614 N{feeder}`

* `{feeder}` is the Feeder to enable.

### Disable Feeder (M615)

`M615 N{feeder}`

* `{feeder}` is the Feeder to disable.
