# xx3dsfml

xx3dsfml is a multi-platform capture program for [3dscapture's](https://3dscapture.com/) N3DSXL capture board written in C/C++.

#### Features

- Lightweight, low latency, and minimalistic design that appears to run reasonably accurately and stably.
- The ability to split the screens into separate windows or join them into a single window.
- The ability to evenly and incrementally scale the windows independently of each other.
- The ability to rotate the windows independently of each other to either side and even upside down.
- The ability to crop the windows independently of each other by game system in both their scaled and native resolutions where applicable.
- The ability to blur the contents of the windows independently of each other.
- The ability to both darken and lighten the screen and also quickly return to the standard default brightness.
- Smooth, continuous volume controls with separate mute control.
- A config file that saves all of these settings individually which allows all three windows to have completely different configurations.
- 12 configurable user layouts that can be saved to and loaded from on the fly.

_Note: Games for other systems boot in scaled resolution mode by default. Holding START or SELECT while launching these games will boot in native resolution mode._

#### Dependencies

xx3dsfml has two dependencies: [FTDI's D3XX driver](https://ftdichip.com/drivers/d3xx-drivers/) and [SFML](https://www.sfml-dev.org/).

The D3XX driver can be installed along with the program itself using the provided Makefile as outlined in the __Install__ section below. As of now, this is the required way to install the driver in order to fully support this program. This is because the latest driver (1.0.14) is bugged, so the previous version (1.0.5) will be installed instead. The latest version can still be used, but it will be unstable and prone to crashing in certain circumstances.

The native C++ version of SFML, including its development files, also needs to be installed. The simplest way to accomplish this would be using a package manager, which [Homebrew](https://brew.sh/) is a popular choice for on macOS.

_Note: C++ is the default language for SFML and is not a binding._

#### Install

Installing xx3dsfml is as simple as compiling the xx3dsfml.cpp code. A Makefile is provided with the following functionality:

- `make`:               This will build the xx3dsfml executable locally, which can be executed via the `./xx3dsfml` command from the directory where it resides. This requires the D3XX driver to already be installed.
- `make clean`:         This will remove all files, including the local xx3dsfml executable, created by the above command.
- `make ftd3xx`:        This will install the D3XX driver, including its development files.
- `make install`:       This will build and install the xx3dsfml executable systemwide along with the D3XX driver, including its development files. This xx3dsfml executable can be executed via the `xx3dsfml` command from any directory.
- `make uninstall`:     This will uninstall the systemwide xx3dsfml executable along with the D3XX driver, including its development files.
- `make update`:        This will download the latest versions of the LICENSE, Makefile, README.md, and xx3dsfml.cpp files.

When using any of these commands, you may be required to have root (admin) privileges. This can be achieved by prepending these commands with the `sudo` command and entering your password when prompted. On macOS, you may also be prompted to install the Apple Command Line Developer Tools first. Additionally, on macOS, a command line capable version of 7-Zip is required at this time. This is because the previous version of the D3XX driver (1.0.5) is only available as a DMG file, which 7-Zip is capable of extracting from. If, for whatever reason, compiling the xx3dsfml.cpp code fails even after installing the dependencies, a system reboot may be required first before attempting to compile it again.

#### Controls

- __Escape key__:       Toggles the logical connection to the N3DSXL. The N3DSXL is logically connected by default if physically connected at runtime and will be logically disconnected if physically disconnected during runtime. This control is bypassed when the `--auto` flag is set as outlined in the __Arguments__ section below.
- __Tab key__:          Swaps between split mode and joint mode which splits the screens into separate windows or joins them into a single window respectively.
- __0 key__:            Returns the brightness to its default of 100.
- __- key__:            Decrements the brightness by 5. 50 is the minimum.
- __= key__:            Increments the brightness by 5. 150 is the maximum.
- __B key__:            Toggles blurring on/off for the focused window. This is only noticeable at 1.5x scale or greater.
- __Down key__:         Decrements the scaling by 0.5x for the focused window. 1.0x is the minimum.
- __Up key__:           Increments the scaling by 0.5x for the focused window. 4.5x is the maximum.
- __Left key__:         Rotates the focused window 90 degrees counterclockwise.
- __Right key__:        Rotates the focused window 90 degrees clockwise.
- __[ key__:            Cycles to the previous cropping mode for the focused window. The currently supported cropping modes are for default 3DS, scaled DS, and native DS respectively.
- __] key__:            Cycles to the next cropping mode for the focused window. The currently supported cropping modes are for default 3DS, scaled DS, and native DS respectively.
- __M key__:            Toggles mute on/off.
- __, key__:            Decrements the volume by 5 units. 0 is the minimum. Adjusting the volume won't cause the audio to unmute.
- __. key__:            Increments the volume by 5 units. 100 is the maximum. Adjusting the volume won't cause the audio to unmute.
- __F1 - F12 keys__:    Loads from layouts 1 through 12 respectively, and while holding __Ctrl__, saves to layouts 1 through 12 respectively.

_Note: The volume is independent of the actual volume level set with the physical slider on the 3DS, and the brightness is independent of the actual brightness set in the options menu of the 3DS._

#### Settings

When starting the program for the first time, a message indicating a load failure for the xx3dsfml.conf file will be displayed, and the same will occur when attempting to load from any given layout file if it hasn't been saved to before. These files must be created by the program first before they can be loaded from. When successfully closed, the program saves its current configuration to the xx3dsfml.conf file, creating the file if it doesn't already exist, and loads from it at startup.

Just as well, the current configuration can be saved to any of the 12 layout files at any time using keys F1 through F12 while holding Ctrl, creating the given file if it doesn't already exist, which can then be loaded from at any time using keys F1 through F12 without holding Ctrl respectively. Changing the configuration after a layout is loaded will not overwrite it unless the respective save function is used after the changes are made.

_Note: Controls that target the individual windows are saved and loaded independently of each other, meaning that settings for the single window in joint mode as well as the separate windows in split mode are all individually stored in these files._

#### Arguments

The following command line arguments are currently available when running the xx3dsfml executable:

- `--auto`:     Runs the program in auto-connect mode. When the N3DSXL is disconnected, the program will attempt to reconnect to it automatically every 5 seconds. This mode disables the C key as outlined in the __Controls__ section above.
- `--safe`:     Runs the program in safe mode. Settings cannot be loaded from or saved to the config or layout files when in this mode, forcing the program to use the internal defaults instead.
- `--vsync`:    Runs the program in vsync mode. By default, the program runs with a frame rate limit of 60 FPS, matching the 3DS itself. Using this option will force the program to run with a frame rate limit that matches the refresh rate of the monitor, which may lead to a decrease in system performance. There may also be issues on some systems if any of the windows are obscured, even just partially, when running in this mode, but this is something that I've never experienced myself.

_Note: Multiple runtime flags can be used at a time and can even be aliased in a system command if so desired._

#### Notes

- Minimal audio artifacts can occur in certain scenarios, and the audio can vary slightly in latency. This can be due to a number of factors including high system load as well as low processing priority, but even under ideal conditions, some degree of audio latency will still exist. This is partially due to the 3DS's non-integer sample rate as well as how it's captured by the hardware but is mostly due to how SFML currently implements streamed audio playback.
- Switching audio devices while the program is running, though considered bad practice, should be okay. If the audio doesn't switch over to the new output device, logically reconnecting the N3DSXL should force it to change. Frankly, this is really something that should just be handled internally by SFML in the first place.
- Switching graphics devices while the program is running is something I shouldn't even need to write about here. You're smarter than that, right?
- If the program is ever unable to create a handle to the N3DSXL at startup even though it's connected to and recognized by the system, physically reconnecting it and restarting the program should resolve the issue.
- If any issues occur while the N3DSXL is indirectly connected to the system that isn't resolved by reconnecting it and restarting the program, please consider connecting it directly to the system instead.
- If the N3DSXL cannot be logically connected no matter the case, it may be due to the user having insufficient permissions to access the USB device. This is a common, albeit system dependent, issue for which a general solution should be applicable.
- On some systems, the audio playback may be choppy or crackly. This is likely due to how much priority the system is giving the program and its processes. In order for real time audio to be low latency, it needs to be processed as quickly as reasonably possible. This sort of issue is common amongst audio software that requires low latency processing, so a general solution, depending on the system, should be applicable in this case.

#### Media

![xx3dsfml](xx3dsfml.png "xx3dsfml")
