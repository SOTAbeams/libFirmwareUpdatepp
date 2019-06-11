# libFirmwareUpdate++

A C++ library for downloading firmware updates to devices.

It currently only supports DFU downloads, based on heavily modified code from [dfu-util](http://dfu-util.sourceforge.net/). The license is therefore the same as dfu-util (GPLv2).

This library was created because making a good GUI frontend to dfu-util is difficult, due to the limited access to progress reporting and error logging provided by the command line nature of dfu-util. Turning it into a library makes it easier to access these details.

It may also allow useful things like a program to download an update to multiple devices at once to be made (difficult with dfu-util directly since it refuses to do more than one device at a time).

Some work towards ABI stability by decoupling interface and implementation has been done, but this is incomplete and the interface should not be considered stable. This library is therefore best linked statically at the moment.

## Dependencies
* cmake
* C++ compiler supporting C++14
* libusb

## Todo
* Rewrite from scratch? Basing this library on dfu-util made it quicker to get everything working, but also limits licensing (has to be GPLv2, not GPLv2+), and the code still isn't organised particularly neatly.
* Add automated tests where they would be useful and practical
* Replicate more features from DFU? At the moment, it has enough of them to to DfuSe downloads.
* Is it possible to use the drivers from ST for DfuSe on Windows, instead of having to switch to a WinUsb/libusb driver? (low priority)
