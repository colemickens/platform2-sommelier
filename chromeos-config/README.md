# Chrome OS Configuration -- Master Chrome OS Configuration tools / library

This is the homepage/documentation for chromeos-config which provides access to
the master configuration for Chrome OS.

[TOC]

## Internal Documentation

See the [design doc](http://go/cros-unified-build-design) for information about
the design. This is accessible only within Google. A public page will be
published to chromium.org once the feature is complete and launched.

## Important classes

See [CrosConfig](./libcros_config/cros_config.h) for the class to use to access
configuration strings.

## Binding

This section describes the binding for the master configuration. This defines
the structure of the configuration and the nodes and properties that are
permitted.

In Chromium OS, the word 'model' is used to distinguish different hardware or
products for which we want to build software. A model shares the same hardware
and the same brand. Typically two different models are distinguished by hardware
variations (e.g. different screen size) or branch variations (a different OEM).

Note: In the description below, entries with children are nodes and leaves are
properties.

*   `family`: Provides family-level configuration settings, which apply to all
    models in the family.

    *   `firmware` (optional) : Contains information about firmware versions and
        files
        *   `script`: Updater script to use. See [the pack_dist
            directory](https://cs.corp.google.com/chromeos_public/src/platform/firmware/pack_dist)
            for the scripts. The options are:
            *   `updater1s.sh`: Only used by mario. Do not use for new boards.
            *   `updater2.sh`: Only used by x86-alex and x86-zgb. Do not use for
                new boards.
            *   `updater3.sh`: Used for various devices shipped around 2012.
            *   `updater4.sh`: In current use. Supports software sync for the
                EC.
            *   `updater5.sh`: In current use. Supports firmware v4
                (chromeos-ec, vboot2)
        *   `shared`: Contains information intended to be shared across all
            models (see firmware discussion under models below)
            *   `bcs-overlay`: Overlay name containing the firmware binaries.
                This is used to generate the full path. For example a value of
                `overlay-reef-private` in the `reef` model means that all files
                will be of the form
                `gs://chromeos-binaries/HOME/bcs-reef-private/overlay-reef-private/chromeos-base/chromeos-firmware-reef/<filename>`.
            *   `build-targets`: Sub-nodes of this define the name of the build
                artifact produced by a particular software project in the
                Portage tree.
                *   `coreboot`: Defines the Kconfig/target used for coreboot and
                    chromeos-bootimage ebuilds.
                *   `ec`: Defines the "board" used to generate the ec firmware
                    blob within the chromeos-ec ebuild.
                *   `depthcharge`: Defines the model target passed to the
                    compile phase within the depthcharge ebuild.
                *   `libpayload`: Not currently used as the libpayload ebuild is
                    not yet unibuild-aware.
            *   `main-image`: Main image location. This must start with `bcs://`
                . It refers to a file available in BCS. The file will be
                unpacked to produce a firmware binary image.
            *   `main-rw-image` (optional): Main RW (Read/Write) image location.
                This must start with `bcs://`. It refers to a file available in
                BCS. The file will be unpacked to produce a firmware binary
                image.
            *   `ec-image` (optional): EC (Embedded Controller) image location.
                This must start with `bcs://` . It refers to a file available in
                BCS. The file will be unpacked to produce a firmware binary
                image.
            *   `pd-image` (optional): PD (Power Delivery controller) image
                location. This must start with `bcs://` . It refers to a file
                available in BCS. The file will be unpacked to produce a
                firmware binary image.
            *   `stable-main-version` (optional): Version of the stable
                firmware. On dogfood devices where RO firmware can be updated,
                we perform a full firmware update if the existing firmware on
                the device is older than this version. *Deprecation in progress.
                See crbug.com/70541.*
            *   `stable-ec-version` (optional): Version of the stable EC
                firmware. On dogfood devices where RO EC firmware can be
                updated, we perform a full firmware update if the existing EC
                firmware on the device is older than this version. *Deprecation
                in progress. See crbug.com/70541.*
            *   `stable-pd-version` (optional): Version of the stable PD
                firmware. On dogfood devices where RO PD firmware can be
                updated, we perform a full firmware update if the existing PD
                firmware on the device is older than this version. *Deprecation
                in progress. See crbug.com/70541.*
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

*   `models`: Sub-nodes of this define models supported by this board.

    *   `<model name>`: actual name of the model being defined, e.g. `reef` or
        `pyro`
        *   `thermal`(optional): Contains information about thermel properties
            and settings.
            *   `dptf-dv': Filename of the .dv file containing DPTF (Dynamic
                Platform and Thermal Framework) settings, relative to the
                ebuild's FILESDIR.
        *   `wallpaper` (optional): base filename of the default wallpaper to
            show on this device. The base filename points `session_manager` to
            two files in the `/usr/share/chromeos-assets/wallpaper/<wallpaper>`
            directory: `/[filename]_[small|large].jpg`. If these files are
            missing or the property does not exist, "default" is used.
        *   `firmware` (optional) : Contains information about firmware versions
            and files. The properties and nodes inside this node are exactly the
            same as family/firmware/shared. By convention, tools looking for
            firmware properties for a model will fallback to the family-level
            firmware/shared configuration if the node or property is not found
            at the model level.
            *   `shares`(optional): Phandle pointing to the firmware to use for
                this model. This is a list with a single phandle, pointing to
                the firmware node of another model. The presence of this
                property indicates that this model does not have separate
                firmware although it may have its own keyset. This property is
                used to share firmware across multiple models where hardware
                differences are small and we can detect the model from board ID
                pins. At this time, only a phandle reference to a node at
                family/firmware/shared is supported. The phandle target node
                must be named with a valid model (e.g. 'reef').
            *   `key-id` (optional): Unique ID that matches which key
                will be used in for firmware signing as part of vboot.
                For context, see go/cros-unibuild-signing
        *   `powerd-prefs` (optional): Name of a subdirectory under the powerd
            model_specific prefs directory where model-specific prefs files are
            stored.

### Example for reef

```
chromeos {
    family {
        firmware {
            script = "updater4.sh";
            shared: reef {
                bcs-overlay = "overlay-reef-private";
                main-image = "bcs://Reef.9042.50.0.tbz2";
                ec-image = "bcs://Reef-EC.9042.43.0.tbz2";
                stable-main-version = "Google-Reef.9042.43.0";
                stable-ec-version = "reef-v1.1.5840-f0d7761";
                extra = "${FILESDIR}/extra",
                    "${SYSROOT}/usr/sbin/ectool",
                    "bcs://Reef.something.tbz";
                build-targets {
                    coreboot = "reef";
                    ec = "reef";
                    depthcharge = "reef";
                    libpayload = "reef";
                };
            };
            pinned_version: sand {
                bcs-overlay = "overlay-reef-private";
                main-image = "bcs://Reef.9041.50.0.tbz2";
                ec-image = "bcs://Reef-EC.9041.43.0.tbz2";
                stable-main-version = "Google-Reef.9041.43.0";
                stable-ec-version = "reef-v1.1.5840-f0d7761";
                extra = "${FILESDIR}/extra",
                    "${SYSROOT}/usr/sbin/ectool",
                    "bcs://Reef.something.tbz";
                build-targets {
                    coreboot = "reef";
                    ec = "reef";
                    depthcharge = "reef";
                    libpayload = "reef";
                };
            };
        };
    };

    models {
        reef {
            powerd-prefs = "reef";
            wallpaper = "seaside_life";
            firmware {
                shares = <&shared>;
                key-id = "reef";
            };
            thermal {
                dptf-dv = "reef/dptf.dv";
            };
        };

        pyro {
            powerd-prefs = "pyro_snappy";
            wallpaper = "alien_invasion";
            firmware {
                bcs-overlay = "overlay-pyro-private";
                main-image = "bcs://Pyro.9042.41.0.tbz2";
                ec-image = "bcs://Pyro_EC.9042.41.0.tbz2";
                key-id = "pyro";
                build-targets {
                    coreboot = "pyro";
                    ec = "pyro";
                    depthcharge = "pyro";
                    libpayload = "reef";
                };
            };
            thermal {
                dptf-dv = "pyro/dptf.dv";
            };
        };

        snappy {
            powerd-prefs = "pyro_snappy";
            wallpaper = "chocolate";
            firmware {
                bcs-overlay = "overlay-snappy-private";
                main-image = "bcs://Snappy.9042.43.0.tbz2";
                ec-image = "bcs://Snappy_EC.9042.43.0.tbz2";
                key-id = "snappy";
                build-targets {
                    coreboot = "snappy";
                    ec = "snappy";
                    depthcharge = "snappy";
                    libpayload = "reef";
                };
            };
        };

        basking {
            powerd-prefs = "reef";
            wallpaper = "coffee";
            firmware {
                shares = <&shared>;
                key-id = "basking";
            };
        };

        sand {
            powerd-prefs = "reef";
            wallpaper = "coffee";
            firmware {
                shares = <&pinned_version>;
                key-id = "sand";
            };
        };

        electro {
            powerd-prefs = "reef";
            wallpaper = "coffee";
            firmware {
                shares = <&pinned_version>;
                key-id = "electro";
            };
        };
    };
};
```
## Usage Instructions

### Pinning Firmware Versions for Specific Models

In order to pin firmware for a single model, change the main-image and
ec-image properties in that image. See `snappy` above as an example.

In order to pin firmware versions for several models and avoid entering the
same information twice, create a new firmware instance pointing to the pinned
rev and then update the repective model's shares phandle to point to the
pinned revision.

In the example above, this is shown using sand (a model) referencing
the pinned firmware.

This pinned firmware can then be shared as normal (e.g. electro in
the example).

This will cause the different version to get installed under a
different models sub-directory in the shellball.
Which achieves the same effect of having 2 separate revisions
(in a slightly round about way) installed in the shellball.

The shellball generated will contain the following (based on
the example above) for the firmware pinning case:
* models/
  * reef/
    * bios.bin
    * ec.bin
    * setvars.sh
  * basking/
    * setvars.sh (points to models/reef/...)
  * sand/
    * bios.bin (a different version)
    * ec.bin (a different version)
    * setvars.sh
  * electro/
    * setvars.sh (points to models/sand/...)
