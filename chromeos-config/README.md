# Chrome OS Configuration -- Master Chrome OS Configuration tools / library

This is the homepage/documentation for chromeos-config which provides access
to the master configuration for Chrome OS.

[TOC]

## Internal Documentation

See the [design doc](http://go/cros-unified-build-design) for information about
the design. This is accessible only within Google. A public page will be
published to chromium.org once the feature is complete and launched.

## Important classes

See [CrosConfig](./libcros_config/cros_config.h) for the class to use to
access configuration strings.

## Binding

This section describes the binding for the master configuration. This defines
the structure of the configuration and the nodes and properties that are
permitted.

In Chromium OS, the word 'model' is used to distinguish different hardware or
products for which we want to build software. A model shares the same hardware
and the same brand. Typically two different models are distinguished by
hardware variations (e.g. different screen size) or branch variations (a
different OEM).

Note: In the description below, entries with children are nodes and leaves are
properties.

*   `models`: Sub-nodes of this define models supported by this board.

    * `<model name>`: actual name of the model being defined, e.g. `reef` or
        `pyro`
        *   `wallpaper` (optional): base filename of the default wallpaper to
            show on this device. The base filename points `session_manager` to
            two files in the `/usr/share/chromeos-assets/wallpaper/<wallpaper>`
            directory: `/[filename]_[small|large].jpg`. If these files are
            missing or the property does not exist, "default" is used.
        *   `firmware` (optional) : Contains information about firmware
            versions and files
            *   `bcs-overlay`: Overlay name containing the firmware binaries.
                This is used to generate the full path. For example a value of
                `overlay-reef-private` in the `reef` model means that all files
                will be of the form
                `gs://chromeos-binaries/HOME/bcs-reef-private/overlay-reef-private/chromeos-base/chromeos-firmware-reef/<filename>`.
            *   `main-image`: Main image location. This must start with
                `bcs://` . It refers to a file available in BCS. The file will
                be unpacked to produce a firmware binary image.
            *   `script`: Updater script to use. See
                [the pack_dist directory](https://cs.corp.google.com/chromeos_public/src/platform/firmware/pack_dist)
                for the scripts. The options are:
                *   `updater1s.sh`: Only used by mario. Do not use for new
                    boards.
                *   `updater2.sh`: Only used by x86-alex and x86-zgb. Do not
                    use for new boards.
                *   `updater3.sh`: Used for various devices shipped around 2012.
                *   `updater4.sh`: In current use. Supports software sync for
                    the EC.
                *   `updater5.sh`: In current use. Supports firmware v4
                    (chromeos-ec, vboot2)
            *   `main-rw-image` (optional): Main RW (Read/Write) image
                location. This must start with `bcs://`. It refers to a file
                available in BCS. The file will be unpacked to produce a
                firmware binary image.
            *   `ec-image` (optional): EC (Embedded Controller) image location.
                This must start with `bcs://` . It refers to a file available
                in BCS. The file will be unpacked to produce a firmware binary
                image.
            *   `pd-image` (optional): PD (Power Delivery controller) image
                location. This must start with `bcs://` . It refers to a file
                available in BCS. The file will be unpacked to produce a
                firmware binary image.
            *   `stable-main-version` (optional): Version of the stable
                firmware. On dogfood devices where RO firmware can be updated,
                we perform a full firmware update if the existing firmware on
                the device is older than this version.
            *   `stable-ec-version` (optional): Version of the stable EC
                firmware. On dogfood devices where RO EC firmware can be
                updated, we perform a full firmware update if the existing EC
                firmware on the device is older than  this version.
            *   `stable-pd-version` (optional): Version of the stable PD
                firmware. On dogfood devices where RO PD firmware can be
                updated, we perform a full firmware update if the existing PD
                firmware on the device is older than this version.
            *   `extra` (optional): A list of extra files or directories needed
                to update firmware, each being a string filename. Any filename
                is supported. If it starts with `bcs://` then it is read from
                BCS as with main-image above. But normally it is a path. A
                typical example is `${FILESDIR}/extra` which means that the
                `extra` diectory is copied from the firmware ebuild's
                `files/extra` directory. Full paths can be provided, e.g.
                `${SYSROOT}/usr/bin/ectool`. If a directory is provided, its
                contents are copied (subdirectories are not supported). This
                mirrors the functionality of `CROS_FIRMWARE_EXTRA_LIST`. But
                note that multiple files or directories should use a normal
                device-tree list format, not be separated by semicolon.
            *   `tools` (optional): A list of additional tools which should be
                packed into the firmware update shellball. This is only needed
                if this model needs to run a special tool to do the firmware
                update.
            *   `create-bios-rw-image` (optional): If present this indicates
                that we should re-sign and generate a read-write firmware image.
                This replaces the `CROS_FIRMWARE_BUILD_MAIN_RW_IMAGE` ebuild
                variable.

### Example for reef

```
chromeos {
    models {
        reef {
            wallpaper = "seaside_life";
            firmware {
                bcs-overlay = "overlay-reef-private";
                main-image = "bcs://Reef.9042.50.0.tbz2";
                ec-image = "bcs://Reef-EC.9042.43.0.tbz2";
                stable-main-version = "Google-Reef.9042.43.0";
                stable-ec-version = "reef-v1.1.5840-f0d7761";
                script = "updater4.sh";
                extra = "${FILESDIR}/extra",
                    "${SYSROOT}/usr/sbin/ectool",
                    "bcs://Reef.something.tbz";
            };
        };

        pyro {
            wallpaper = "alien_invasion";
            firmware {
                bcs-overlay = "overlay-pyro-private";
                main-image = "bcs://Pyro.9042.41.0.tbz2";
                ec-image = "bcs://Pyro_EC.9042.41.0.tbz2";
                script = "updater4.sh";
            };
        };

        snappy {
            wallpaper = "chocolate";
            firmware {
                bcs-overlay = "overlay-snappy-private";
                main-image = "bcs://Snappy.9042.43.0.tbz2";
                ec-image = "bcs://Snappy_EC.9042.43.0.tbz2";
                script = "updater4.sh";
            };
        };
    };
};
```
