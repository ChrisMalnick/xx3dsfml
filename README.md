# **xx3dsfml**

xx3dsfml is a multi-platform capture program for [3dscapture's](https://3dscapture.com/) N3DSXL capture board written in C/C++.

#### Features

- Lightweight, low latency, and minimalistic design that appears to run reasonably accurately and stably.
- The ability to split the screens into separate windows or join them into a single window.
- The ability to evenly and incrementally scale the windows independently of each other.
- The ability to rotate the windows independently of each other to either side and even upside down.
- The ability to crop the windows independently of each other for DS games.
- The ability to blur the contents of the windows independently of each other.
- Smooth, continuous volume controls with separate mute control.
- A config file that saves all of these settings individually which allows all three windows to have completely different configurations.
- Four configurable user layouts that can be saved to and loaded from on the fly.

#### Dependencies

*Note: The following instructions are for Linux.*

xx3dsfml has two dependencies, [FTDI's D3XX driver](https://ftdichip.com/drivers/d3xx-drivers/) and [SFML](https://www.sfml-dev.org/).

The D3XX driver can be downloaded from the link above which also contains the installation instructions. However, in order to compile the xx3dsfml.cpp code, two additional steps need to be taken:

1. A directory named libftd3xx needs to be created in the /usr/include directory.
2. The ftd3xx.h and Types.h header files need to be copied to the newly created libftd3xx directory.

Doing this is the equivalent of installing a development package for a utility via a package manager and will allow **any** C/C++ code/compilers to reference these headers.

The SFML **development** package for **C++** also needs to be installed. C++ is the default language for SFML and is not a binding. This can very likely be installed via your package manager of choice.

#### Install

Installing xx3dsfml is as simple as compiling the xx3dsfml.cpp code. A Makefile utilizing the Make utility and g++ compiler is provided with the following functionality:

1. make:	    This will create the executable which can be executed via the ./xx3dsfml command.
2. make clean:	This will remove all files (including the executable) created by the above command.

#### Controls

- S key: Swaps between split mode and joint mode which splits the screens into separate windows or joins them into a single window respectively.
- C key: Toggles cropping on/off for the focused window. This can be used to fill the screen when the console is in DS mode.
- B key: Toggles blurring on/off for the focused window. This is only noticeable at 1.5x scale or greater.
- \- key: Decrements the scaling by 0.5x for the focused window. 1.0x is the minimum.
- = key: Increments the scaling by 0.5x for the focused window. 4.5x is the maximum.
- [ key: Rotates the focused window 90 degrees counterclockwise.
- ] key: Rotates the focused window 90 degrees clockwise.
- M key: Toggles mute on/off.
- , key: Decrements the volume by 5 units. 0 is the minimum.
- . key: Increments the volume by 5 units. 100 is the maximum.
- F1 - F4 keys: Loads from layouts 1 through 4 respectively.
- F5 - F8 keys: Saves to layouts 1 through 4 respectively.

*Note: The volume is independent of the actual volume level set with the physical slider on the console.*

#### Settings

A config file is provided which contains default program settings for the above mentioned controls. These settings are loaded when the program is started and saved when the program is closed. Controls that target the individual windows are saved and loaded independently of each other, meaning that settings for the single window in joint mode as well as the separate windows in split mode are all individually stored in this file.

Just as well, four layout files are provided which can be used to save and load configurations quickly and easily. At any time, keys F5 through F8 can be used to save the current configuration to their respective layouts which can then be loaded at any time using the respective keys F1 through F4. Changing the configuration after a layout is loaded will not overwrite it, and the only way to do so would be to press the respective save key after the changes are made. Whatever the configuration is when the program is closed is what will be saved to the config file and loaded when the program is next opened.

#### Notes

- An N3DSXL must be connected when launching the program or else it will close immediately. This doesn't apply to sleep mode.
- Disconnecting the N3DSXL while the program is running will cause it to close immediately. This doesn't apply to sleep mode.
- Minimal audio artifacts can occur, albeit very infrequently, and the audio can vary slightly in latency. This is due to the 3DS's non-integer sample rate.
- The Ofast g++ optimize flag has been added to the Makefile, so when compiling without it, be aware that optimizations may not occur unless explicitly specified.
- The xx3dsfml.conf file and presets directory must be kept in the same directory as the xx3dsfml executable, and the xx3dsfml executable must be executed from the same directory where the xx3dsfml.conf file and presets directory reside. This is because the config and layout files are referenced relative to the executable, meaning that they will not be located by the program in either of these scenarios.
- Occasionally, the software may be unable to create a handle to the device at startup even though it's connected to and recognized by the system. Simply reconnecting the device and restarting the software should resolve the issue.
- If any issues occur while the device is indirectly connected to the system that isn't resolved by reconnecting the device and restarting the software, please consider connecting the device directly to the system instead.

#### Media

![xx3dsfml](xx3dsfml.png "xx3dsfml")
