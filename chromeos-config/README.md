# Chrome OS Configuration -- Master Chrome OS Configuration tools / library

This is the homepage/documentation for chromeos-config which provides access
to the master configuration for Chrome OS.

[TOC]

# Internal Documentation

See the [design doc](go/cros-unified-build-design) for information about the
design. This is accessible only within Google. A public page will be publised
to chromium.org once the feature is complete and launched.

# Important classes

See [CrosConfig](./libcros_config/cros_config.h) for the class to use to
access configuration strings.

# Binding

This section describes the binding for the master configuration. This defines
the structure of the configuration and the nodes and properties that are
permitted.

In Chromium OS, the word 'model' is used to distinguish different hardware or
products for which we want to build software. A model shares the same hardware
and the same brand. Typically two different models are distinguished by
hardware variations (e.g. different screen size) or branch variations (a
different OEM).

## Top-level node:

* name: "models"

Sub-nodes define models supported by this board.

## 'Model' node:

* name: Name of device
* Required properties:
  * wallpaper: base filename of the default wallpaper to show on this device.
The base filename points the loging manager to 2 files in the
`/usr/share/chromeos-assets/wallpaper/<wallpaper>` directory:
`/[filename]_[small|large].jpg`. If these files are missing or the property
does not exist, "default" is used.

## Example for reef

```
chromeos {
	models {
		reef {
			wallpaper = "seaside_life";
		};
		pyro {
			wallpaper = "alien_invasion";
		};
	snappy {
			wallpaper = "chocolate";
	};
};
```
