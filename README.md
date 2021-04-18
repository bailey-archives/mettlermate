# MettlerMate

## Introduction

This is a simple filter driver that makes Mettler Toledo BC60 scales work with PostalMate. It's also a great template
for an HID USB filter driver that intercepts and modifies read reports, descriptors, and more.

The Mettler Toledo BC60 series is supposed to be fully backwards compatible with the PS60. However, a small programming
error in PostalMate prevents the software from reading the correct weight in lb-oz unit mode.

Their support team insisted it was the scale's fault. Here is a driver to prove otherwise.

## Installation

Follow these steps in order to install the driver for development. This project is not intended for use on commercial
machines at this time, but if you're able to sign the driver, then go right ahead!

**Preparation**

- [Disable driver signature enforcement](https://www.howtogeek.com/167723/how-to-disable-driver-signature-verification-on-64-bit-windows-8.1-so-that-you-can-install-unsigned-drivers/)
- Download the repository and open the solution file.

**Building**

- Build the driver in debug mode with arch set to x64.
- Locate the built driver files (cat, inf, and sys extensions).

**Driver installation**

- On the target machine, right click the INF file and choose "install".
- Open device manager and right click the "HID-compliant weighing device" under Human Interface Devices. Choose "update
  driver".
- Choose "browse my computer" and "let me pick from a list of available drivers".
- Locate the MettlerMate driver in the list and select it.

You may be prompted to restart the machine the first time you load the driver.

## Customization

It's simple to customize this driver for your own device. First edit the `MettlerMate.inf` file to replace the
hardware ID on line 33 with the `HID\` identifier for your particular device. This is required otherwise you will not
be able to install the driver from device manager. You can optionally change the device name to anything you want.

Then edit the `driver.h` file and change the `_WEIGHT_REPORT` struct to match the data structure that your device sends.

Finally edit the `driver.c` file according to the following:

- Use the `FilterEvtIoDeviceControl` function to edit report descriptors. This is necessary if you're going to change
  the size (in bytes) of data. Microsoft has
  [some example code](https://github.com/microsoft/Windows-driver-samples/blob/master/hid/vhidmini2/driver/vhidmini.c#L314)
  to help start on this.

- Use the `FilterRequestCompletionRoutine` function to view read report buffers in transit from the device to userland.

- Use the `ConvertWeightBuffer` function to edit read report buffers to fit your needs.
