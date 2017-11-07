# Ippusb Manager

This is the documentation for `ippusb_manager` which is a service needed to
support ipp-over-usb printing in Chrome OS.

## Overview

The `ippusb_manager` service assists with establishing communication between
cups and ippusbxd in order to print using ipp-over-usb.

When `CUPS` tries to print using the `ippusb` scheme, it first queries
`ippusb_manager` to verify that the given printer is currently connected on the
system. The `ippusb_manager` searches for the given printer, and if it is found
then it launches and instance of the ippusbxd program, and responds to cups with
the name of the socket it can use to communicate with ippusbxd.

In order to prevent `ippusb_manager` from having to constantly be running, the
upstart-socket-bridge is used to start the manager whenever its socket receives
a query from cups.

## Internal Documentation

See the [design doc](http://go/ipp-over-usb) for information about the overall
design and how `ippusb_manager` fits into it. This is accessible only within
google.

## Code Overview

This repository contains the following subdirectories:

| Subdirectory | Description |
|----------------------------|
| `etc/init`   | Upstart config files for launching `ippusb_manager` |
| `udev/`      | udev rules for setting group permissions on ipp-usb printers |
