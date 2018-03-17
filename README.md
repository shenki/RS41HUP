# RS41HUP (Ham Use Project) - Project Horus Fork
Firmware for RS41 for HAM use.

It is possible to recycle RS41-SGP sondes for amateur radio use without any electrical changes! You just have to build a new firmware (this one) and apply it via a cheap programmer, the "ST-Linkv2" (or alternatives). The modified sonde can now transmit on a user-defined frequency in the 70cm band, with Habhub-compatible RTTY telemetry!

Released under GPL v2.

Original Repository: https://github.com/Qyon/STM32_RTTY, though this for is based on [DF8OE's version](https://github.com/df8oe/RS41HUP).

Modifications by Mark Jessop <vk5qi@rfhead.net> include:
* Compatability with existing Project Horus RTTY Formats.
* Support for 4FSK and 2FSK binary telemetry (to be decoded by a future codec2-dev application)
* Removed APRS support - no 70cm APRS infrastructure in Australia, so not really useful to us.


# Compilation
## Linux / OSX:
* Grab the latest GNU ARM Embedded toolchain from here: https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads
* Extract the tarball to somewhere useful. In my case I'm using ~/opt/
* Within the RS41HUP directory:
  * Edit CMakeLists.txt and set the correct path to the un-tar'd directory.
  * `cmake .`
  * `make`


## Windows:
(Note, is likely broken - currently targeting Linux / OSX builds)

Use:
https://www.wyzbee.com/download/Utilities/Software/CoIDE-1.7.8.exe

And:
https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-win32.exe

# Programming
Either:
* Use the ST Micro ST-LINK utility (windows only it seems?), or
* [stlink](https://github.com/texane/stlink) under Linux/OSX (though I haven't had much success with this yet...)

# Configuration
All configs in ```config.h```

* ```CALLSIGN``` RTTY callsign
* ```RTTY_FREQUENCY``` RTTY frequency in MHz
* ```RTTY_DEVIATION``` RTTY shift configurable in 270Hz steps (Note that ths 4FSK mode uses 270 Hz spacing across it's 4 tones)
* ```RTTY_SPEED``` RTTY speed in bauds
* ```RTTY_7BIT``` Use 7 bit RTTY
* ```RTTY_USE_2_STOP_BITS``` use 2 stop bits
* ```TX_POWER``` Power 0-7, (7 means 42.95 mW@434.150 MHz measured on E4406A)
* ```TX_DELAY``` Delay between frames in milliseconds
* ```ALLOW_DISABLE_BY_BUTTON``` Allow disabling device using button


Have a nice day ;)

#Changelog
 * 14.12.2016 - Reverse engineeded connections, initial hard work, resulting in working RTTY by SQ7FJB
 * 07.01.2017 - GPS now using proprietiary UBLOX protocol, more elastic code to set working frequency by SQ5RWU
 * 23.01.2017 - Test APRS code, small fixes in GPS code by SQ5RWU
 * 06.06.2017 - APRS code fix, some code cleanup
 * June 2017 - starting with Linux support, making configuration more flexible by DF8OE
 * March 2018 - Addition of 4FSK mode support by Mark VK5QI


#TODO
 * Temperature and moisture sensor support (temperature should be the easiest to get going first...)
 * Implementing protocol for using external devices on extension header
 * Configuration via extension header (serial connection) without need for reflashing firmware
