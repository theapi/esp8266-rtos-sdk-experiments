# _SPI Master Example_

_This example uses the ESP8266 hspi Master to send data

## How to use example

### Hardware Required

* Connection:

| Signal    |NODE MCU| Master |
|-----------|--------|--------|
| SCLK      | D5     | GPIO14 |
| MOSI      | D7     | GPIO13 |
| CS        | D8     | GPIO15 |
| GND       | GND    | GND    |



### Configure the project

```
make menuconfig
```

* Set serial port under Serial Flasher Options.


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```
