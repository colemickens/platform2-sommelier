# ARC adbd ConfigFS / FunctionFS proxy for Developer Mode

This sets up the ADB gadget to allow Chromebooks that have the necessary
hardware / kernel support to be able to use ADB over USB. This avoids exposing
ConfigFS into the container.

See
https://android.googlesource.com/platform/system/core/+/master/adb/daemon/usb.cpp
for more information.
