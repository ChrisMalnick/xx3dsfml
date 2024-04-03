# xx3dsfml

xx3dsfml is a multi-platform capture program for [3dscapture's](https://3dscapture.com/) N3DSXL capture board written in C/C++.

#### Features

- Lightweight, low latency, and minimalistic design that appears to run reasonably accurately and stably.
- The ability to split the screens into separate windows or join them into a single window.
- The ability to evenly and incrementally scale the windows independently of each other.
- The ability to rotate the windows independently of each other to either side and even upside down.
- The ability to crop the windows independently of each other for DS games in both scaled and native resolution.
- The ability to blur the contents of the windows independently of each other.
- A config file that saves all of these settings individually which allows all three windows to have completely different configurations.
- Four configurable user layouts that can be saved to and loaded from on the fly.

_Note: DS games boot in scaled resolution mode by default. Holding START or SELECT while launching DS games will boot in native resolution mode._
_Note: Make sure the 3DS audio is not set to Surround._

#### Dependencies

xx3dsfml has two dependencies: [FTDI's D3XX driver](https://ftdichip.com/drivers/d3xx-drivers/) and [SFML](https://www.sfml-dev.org/).

The D3XX driver will be automatically downloaded with the provided Makefile. As of writing, it will not be the latest version of the driver. This is because the latest driver (1.0.14) is bugged, so the previous version (1.0.5) will be installed instead. The latest version can still be used, but it will be unstable and prone to crashing in certain circumstances.

The native C++ version of SFML, including its development files, also needs to be installed. The simplest way to accomplish this would be using a package manager, which [Homebrew](https://brew.sh/) is a popular choice for on macOS.

_Note: C++ is the default language for SFML and is not a binding._

#### Compile and Install

Installing xx3dsfml is as simple as compiling the xx3dsfml.cpp code. A Makefile is provided with the following functionality:

1. `make`:                    This will generate the executable.
2. `make clean`:              This will clean the build artifacts.
3. `make install`:            This will install the executable as a systemwide command, which can be called using `xx3dsfml` in the terminal from any directory.
4. `make uninstall`:          This will remove the systemwide executable.

When utilizing the Makefile, you may be prompted for a password when using the install option, and on macOS, you may also be prompted to install the Apple Command Line Developer Tools first.

#### Controls

- __C key__:        Toggles the logical connection to the N3DSXL. The N3DSXL is logically connected by default if physically connected at runtime and will be logically disconnected if physically disconnected during runtime.
- __S key__:        Swaps between split mode and joint mode which splits the screens into separate windows or joins them into a single window respectively.
- __B key__:        Toggles blurring on/off for the focused window. This is only noticeable at 1.5x scale or greater.
- __- key__:        Decrements the scaling by 0.5x for the focused window. 1.0x is the minimum.
- __= key__:        Increments the scaling by 0.5x for the focused window. 4.5x is the maximum.
- __[ key__:        Rotates the focused window 90 degrees counterclockwise.
- __] key__:        Rotates the focused window 90 degrees clockwise.
- __; key__:        Cycles to the next cropping mode for the focused window. The currently supported cropping modes are for default 3DS, scaled DS, and native DS respectively.
- __' key__:        Cycles to the previous cropping mode for the focused window. The currently supported cropping modes are for default 3DS, scaled DS, and native DS respectively.
- __M key__:        Toggles mute on/off.
- __, key__:        Decrements the volume by 5 units. 0 is the minimum.
- __. key__:        Increments the volume by 5 units. 100 is the maximum.
- __F1 - F4 keys__: Loads from layouts 1 through 4 respectively.
- __F5 - F8 keys__: Saves to layouts 1 through 4 respectively.

_Note: The volume is independent of the actual volume level set with the physical slider on the console._

#### Settings

When starting the program for the first time, a message indicating a load failure for the xx3dsfml.conf file will be displayed, and the same will occur when attempting to load from any given layout file if it hasn't been saved to before. These files must be created by the program first before they can be loaded from. When successfully closed, the program saves its current configuration to the xx3dsfml.conf file, creating the file if it doesn't already exist, and loads from it every time at startup.

Just as well, the current configuration can be saved to any of the four layout files at any time using keys F5 through F8, creating the given file if it doesn't already exist, which can then be loaded from at any time using keys F1 through F4 respectively. Changing the configuration after a layout is loaded will not overwrite it unless the respective save key is pressed after the changes are made.

_Note: Controls that target the individual windows are saved and loaded independently of each other, meaning that settings for the single window in joint mode as well as the separate windows in split mode are all individually stored in these files._

#### Arguments

The following command line arguments are currently available when running the xx3dsfml executable:

- `--safe`:     Runs the program in safe mode. Settings cannot be loaded from or saved to the config or layout files when in this mode, forcing the program to use the internal defaults instead.
- `--vsync`:    Runs the program in vsync mode. By default, the program runs with a frame rate limit of 60 FPS, matching the 3DS itself. Using this option will force the program to run with a frame rate limit that matches the refresh rate of the monitor, which may lead to a decrease in system performance.

#### Notes

- Minimal audio artifacts can occur, albeit very infrequently, and the audio can vary slightly in latency. This is due to the 3DS's non-integer sample rate.
- Occasionally, the program may be unable to create a handle to the device at startup even though it's connected to and recognized by the system. Simply reconnecting the device and restarting the program should resolve the issue.
- If any issues occur while the device is indirectly connected to the system that isn't resolved by reconnecting the device and restarting the program, please consider connecting the device directly to the system instead.

#### Media

![xx3dsfml](xx3dsfml.png "xx3dsfml")
