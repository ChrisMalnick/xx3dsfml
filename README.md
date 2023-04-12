# **xx3dsfml**

xx3dsfml is a multi-platform capture program for [3dscapture's](https://3dscapture.com/) New 3DSXL capture card written in C/C++.

#### Dependencies

*Note: The following instructions are for Linux.*

xx3dsfml has two dependencies, [FTDI's D3XX driver](https://ftdichip.com/drivers/d3xx-drivers/) and [SFML](https://www.sfml-dev.org/).

The D3XX driver can be downloaded from the link above which also contains the installation instructions. However, in order to compile the xx3dsfml code, two additional steps need to be taken:

1. A directory named libftd3xx needs to be created in the /usr/include directory.
2. The ftd3xx.h and WinTypes.h header files need to be copied to the newly created libftd3xx directory.

Doing this is the equivalent of installing a development package for a utility via a package manager and will allow **any** C/C++ code/compilers to reference these headers.

The SFML **development** package for **C++** needs to be installed. C++ is the default language for SFML and is not a binding. This can very likely be installed via your package manager of choice.

#### Install

Installing xx3dsfml is as simple as compiling the xx3dsfml.cpp code. A Makefile utilizing the Make utility and g++ compiler is provided with the following functionality:

1. make:	This will create the executable which can be executed via the ./xx3dsfml command.
2. make clean:	This will remove all files (including the executable) created by the above command.

#### Controls

*Note: All of the following numeric controls are accomplished via the Number Row and not the Numeric Keypad.*

When the xx3dsfml program is executed, it will attempt to open a connected New 3DSXL for capture once at start. However, a New 3DSXL can be connected at any time while the software is running but will require it to be manually opened using the 1 key in this case. Pressing the 1 key at any time while a New 3DSXL is open will close it and vice versa. If an open New 3DSXL is disconnected at any time while the software is running, the device will automatically close and should be reopened via the 1 key when reconnected.

The following is a list of controls currently available in the xx3dsfml program:

- 1 key: Opens a connected New 3DSXL if not yet open, otherwise closes a connected New 3DSXL if open.
- 2 key: Toggles smoothing on/off. This is only noticeable at 2x scale or greater.
- 3 key: Decrements the scaling. 1x is the minimum and the default.
- 4 key: Increments the scaling. 4x is the maximum.
- 5 key: Rotates the window 90 degrees counterclockwise.
- 6 key: Rotates the window 90 degrees clockwise.

#### Notes

This software currently lacks audio support and may never get it unfortunately (audio can be captured directly from the AUX port on the New 3DSXL over a line level input though thankfully). The reason for this is that the audio is included at the end of the frame data, but in order to reach it, a feature of the D3XX driver that isn't available outside of Windows likely needs to be used. Future updates could potentially fix this issue.

#### Media

![xx3dsfml](xx3dsfml.png "xx3dsfml")
