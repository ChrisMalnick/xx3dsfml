# **xx3dsfml**

xx3dsfml is a multi-platform capture program for [3dscapture's](https://3dscapture.com/) N3DSXL capture card written in C/C++.

#### Dependencies

*Note: The following instructions are for Linux.*

xx3dsfml has two dependencies, [FTDI's D3XX driver](https://ftdichip.com/drivers/d3xx-drivers/) and [SFML](https://www.sfml-dev.org/).

The D3XX driver can be downloaded from the link above which also contains the installation instructions. However, in order to compile the xx3dsfml.cpp code, two additional steps need to be taken:

1. A directory named libftd3xx needs to be created in the /usr/include directory.
2. The ftd3xx.h and Types.h header files need to be copied to the newly created libftd3xx directory.

Doing this is the equivalent of installing a development package for a utility via a package manager and will allow **any** C/C++ code/compilers to reference these headers.

The SFML **development** package for **C++** also needs to be installed. C++ is the default language for SFML and is not a binding. This can very likely be installed via your package manager of choice.

#### Install

Installing xx3dsfml is as simple as compiling the xx3dsfml.cpp code, making any of the following changes beforehand if necessary:

1. Change the **PRODUCT** macro to **N3DSXL.2** if using the latest board revision.
2. Comment out the **setProcessingInterval** call in the **Audio** class constructor if using a version of SFML earlier than 2.6.0 (which doesn't actually impact the performance very much as it turns out).

A Makefile utilizing the Make utility and g++ compiler is provided with the following functionality:

1. make:	    This will create the executable which can be executed via the ./xx3dsfml command.
2. make clean:	This will remove all files (including the executable) created by the above command.

#### Controls

*Note: All of the following numeric controls are accomplished via the Number Row and not the Numeric Keypad.*

When the xx3dsfml program is executed, it will attempt to open a connected N3DSXL for capture once at start. However, an N3DSXL can be connected at any time while the software is running but will require it to be manually opened using the 1 key in this case. Pressing the 1 key at any time while an N3DSXL is open will close it and vice versa. If an open N3DSXL is disconnected at any time while the software is running, the logical device will automatically close and should be reopened via the 1 key once reconnected.

The following is a list of controls currently available in the xx3dsfml program:

- 1 key: Opens a connected N3DSXL if not yet open, otherwise closes a connected N3DSXL if open.
- 2 key: Toggles smoothing on/off. This is only noticeable at 2x scale or greater. Off is the default.
- 3 key: Decrements the scaling. 1x is the minimum and the default.
- 4 key: Increments the scaling. 4x is the maximum.
- 5 key: Rotates the window 90 degrees counterclockwise.
- 6 key: Rotates the window 90 degrees clockwise.
- 0 key: Toggles mute on/off. Off is the default.
- \- key: Decrements the volume by 5 units. 0 is the minimum.
- = key: Increments the volume by 5 units. 100 is the maximum.

*Note: The volume is set to 10 by default and is independent of the actual volume level set with the physical slider on the console.*

#### Notes

This might only be the case with my particular system, but there's an issue with the audio being captured at a rate that outpaces the playback which results in the amount of queued samples increasing over time. This means that the audio is gradually becoming increasingly out of sync with the video, despite the fact that playback is streamed in a separate thread. As a compromise, I decided to outright omit a sample periodically when the queued sample count exceeds 3. This provides a good balance of reducing the amount of omitted samples while keeping the audio reasonably in sync with the video. If this issue is actually just system dependent, then this compromise won't affect systems that aren't impacted by it since the queued sample count won't ever increase enough for a sample to be dropped in the first place. Even so, the application is more than serviceable in its current state. That being said, this is still a work in progress, and more general features and revisions may also be released in the future.

Also, just to add, the latency in general is very low to the point that I've comfortably played out of an OBS preview that's capturing this software. The audio latency is virtually indistinguishable when listening to the audio output directly from this software while viewing the video feed through something like OBS. I've streamed multiple times now for hours on end with no issues outside of the occasional dropped audio sample which I find to be pretty insignificant. I actually experience the same audio latency issue with my upscaler and capture card which I find to be much more intrusive since I have to continually refresh the audio stream every time the latency becomes too noticeable. I honestly wish there was a way to periodically drop samples from audio sources in OBS just the same as I've done in my software here!

#### Media

![xx3dsfml](xx3dsfml.png "xx3dsfml")
