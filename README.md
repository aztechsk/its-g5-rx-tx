# ITS-G5 Receiver Firmware

## Cloning

```
git clone https://codeberg.org/opentrafficmap/its-g5-receiver-firmware.git
cd its-g5-receiver-firmware
git submodule update --init --recursive
```

## ESP32-C5 Devboard

We recommend using our custom hardware, which can be found in [this repo](https://codeberg.org/opentrafficmap/its-g5-receiver).
However, you can also use a ESP32-C5-WIFI6-KIT devboard.

If you are using our custom hardware, you can skip this section.

If you are using the devboard, make the following connections from the Ethernet module (left) to the ESP32 devboard (right):
```
MOSI <-> GPIO7
MISO <-> GPIO2
SCLK <-> GPIO6
CS   <-> GPIO4
INT  <-> GPIO9
```

Copy the correct sdkconfig file over the default one:
```
# For W5500
cp sdkconfig.proto-w5500 sdkconfig
# or alternatively, for ENC28J60:
cp sdkconfig.proto-enc28j60 sdkconfig
```

If you make any incompatible changes to the hardware/sdkconfig, make sure to set `HW_VARIANT` to `custom` in the sdkconfig,
otherwise you may receive incompatible OTA updates if you connect to the official MQTT server.

## Building

The `install.sh` script needs to be run once to install the ESP32 toolchain files:
```
esp-idf/install.sh
```

To enter the ESP-IDF environment, you need to source the `export.sh` script once per shell:
```
. esp-idf/export.sh
```

Afterwards, you can build the project:
```
idf.py build
```

To flash the firmware and monitor the output, you can use the following command:
```
idf.py -p /dev/ttyACM0 -b 921600 flash monitor -b 115200
```
