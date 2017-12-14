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
configuration strings on a target. See
[cros_config_host.py](./cros_config_host.py) for access to the config on a host
or during a build.

## CLI Usage

There are two CLIs built for Chrome OS configuration access, cros_config for use
on the target, and cros_config_host_py for use on the host/during building. See
the --help for each tool respectively for help on usage.

## Debugging

libcros_config will emit a lot of debugging log messages if you set the
CROS_CONFIG_DEBUG environment variable to a non-empty value before calling into
the library.

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

    *   `audio` (optional): Contains information about audio devices used by
            this family. Each subnode is defined as a phandle that can be
            referenced from the model-specific configuration using the
            `audio-type` property.
        *   `<audio-type>`: Node containing the audio config for one device
                type. All filenames referenced are in relation to the
                ${FILERDIR} directory of the ebuild containing them.
            *   `cras-config-dir`: Directory to pass to cras for the location
                    of its config files
            *   `ucm-suffix`: Internal UCM suffix to pass to cras
            *   `topology-name` (optional): Name of the topology firmware to
                    use
            *   `card`: Name of the audio 'card'
            *   `volume`: Template filename of volume curve file
            *   `dsp-ini`: Template filename of dsp.ini file
            *   `hifi-conf`: Template filename of the HiFi.conf file
            *   `alsa-conf`: Template filename of the card configuration file
            *   `topology-bin` (optional): Template filename of the topology
                    firmware file
        Template filenames may include the following fields, enclosed in
        `{...}` defined by the audio node: `card`, `cras-config-dir`,
        `topology-name`, `ucm-suffix` as well as `model` for the model name.
        The expansion / interpretation happens in cros_config_host. Other users
        should not attempt to implement this. The purpose is to avoid having to
        repeat the filename in each model that uses a particular manufacturer's
        card, since the naming convention is typically consistent for that
        manufacturer.
    *   `arc` (optional): Contains information for the Android container
        used by this family.

        *    `build-properties` (optional): Contains information that will be
             set into the Android property system inside the container.  Each
             subnode is defined as a phandle that can be referenced from the
             model-specific configuration using the `arc-properties-type`
             property. The information here is not to be used for Chrome OS
             itself. It is purely for the container environment.

             The Android build fingerprint is generated from these properties.
             Currently the fingerprint is:

             `google/{product}/{device}:{version_rel}/{id}/{version_inc}:{type}/{tags}`

            Of these, the first three fields come from the properties here. The
            rest are defined by the build system.

            *   `<arc-properties-type>`: Node containing the base cheets
                configuration for use by models.
                *   `product`: Product name to report in `ro.product.name`. This
                    may be the model name, or it can be something else, to allow
                    several models to be grouped into one product.
                *   `device`: Device name to report in `ro.product.device`. This
                    is often `{product}_cheets` but it can be something else if
                    desired.
                *   `oem`: Original Equipment Manufacturer for this model. This
                    generally means the OEM name printed on the device.
                *   `marketing-name`: Name of this model as it is called in the
                    market, reported in `ro.product.model`. This often starts
                    with `{oem} `.
                *   `metrics-tag`: Tag to use to track metrics for this model.
                    The tag can be shared across many models if desired, but
                    this will result in larger granularity for metrics
                    reporting.  Ideally the metrics system should support
                    collation of metrics with different tags into groups, but if
                    this is not supported, this tag can be used to achieve the
                    same end.  This is reported in `ro.product.metrics.tag`.

    *   `power` (optional): Contains information about power devices used by
            this family. Each subnode is defined as a phandle that can be
            referenced from the model-specific configuration using the
            `power-type` property.
        *   `<power-type>`: Node containing the power config for one device
                type. Each property corresponds to a power_manager preference,
                more completely documented in
                [power_manager](https://cs.corp.google.com/chromeos_public/src/platform2/power_manager/common/power_constants.h).
            *   `charging-ports`: String describing charging port positions.
            *   `keyboard-backlight-no-als-brightness`: Initial brightness for
                    the keyboard backlight for systems without ambient light
                    sensors, in the range [0.0, 100.0].
            *   `low-battery-shutdown-percent`: Battery percentage threshold at
                    which the system should shut down automatically, in the
                    range [0.0, 100.0].
            *   `power-supply-full-factor`: Fraction of the battery's total
                    charge at which it should be reported as full in the UI, in
                    the range (0.0, 1.0].
            *   `set-wifi-transmit-power-for-tablet-mode`: If true (1), update
                    wifi transmit power when in tablet vs. clamshell mode.
            *   `suspend-to-idle`: If true (1), suspend to idle by writing
                    freeze to /sys/power/state.

    *   `bcs` (optional): Provides a set of BCS (Binary Cloud Storage) sources
            which can be used to download files needed by the build system.
            Each subnode is defined as a phandle that can be referenced from
            a node which needs access to BCS.
        *   `<bcs-type>`: Node containing information about one BCS tarfile:
            *   `overlay`: Name of overlay to download from
            *   `package`: Package subdirectory to download from
            *   `ebuild-version`: Tarfile version to download. This corresponds
                    to the ebuild version prior to unibuild, but can be any
                    suitable string.
            *   `tarball`: Template for tarball to download. This can include
                `{package}` and `{version}`.

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
                *   `cr50` (optional): Defines the model target to build cr50,
                    if this is used on the platform. This is actually a build
                    target for the EC but is specified separately to make it
                    clear that it is a build for a separate device.
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
            *   `no-firmware` (optional): If present this indicates that this
                model has no firmware at present. This means that it will be
                omitted from the firmware updater (chromeos-firmware-<board>
                ebuild) and it will not be included in the signer instructions
                file sent to the signer. This option is often useful when a
                model is first added, since it may not have firmware at that
                point.

    *   `mapping`: (optional): Used to determine the model/sub-model for a
        particular device. There can be any number of mappings. At present
        only a `sku-map` is allowed.
        *   `sku-map`: Provides a mapping from SKU ID to model/sub-model.
            One of `simple-sku-map` or `single-sku` must be provided.
            `smbios-name-match` is needed only if the family supports
            models which have SKU ID conflicts and needs the SMBIOS name to
            disambiguate them. This is common when migrating legacy boards
            to unified builds, but may also occur if the SKU ID mapping is
            not used for some reason.
            *   `platform-name`: Indicates the platform name for this
                platform. This is reported by 'mosys platform name'. It is
                typically the family name with the first letter capitalized.
            *   `smbios-name-match` (optional) Indicates the smbios name
                that this table mapping relates to. This map will be
                ignored on models which don't have a matching smbios name.
            *   `simple-sku-map` (optional): Provides a simple mapping from
                SKU (an integer value) to model / sub-model. Each entry
                consists of a sku value (typically 0-255) and a phandle
                pointing to the model or sub-model.
            *   `single-sku` (optional): Used in cases where only a single
                model is supported by this mapping. In other words, if the
                SMBIOS name matches, this is the model to use. The value is
                a phandle pointing to the model (it cannot point to a
                sub-model).

    *   `touch` (optional): Contains information about touch devices used by
        this family. Each node is defined as a Phandle that can be referenced
        from the model-specific configuration using the `touch-type` property.
        *   `vendor`: Name of vendor.
        *   `firmware-bin`: Template filename to use for vendor firmware binary.
            The file is installed into `/opt/google/touch`.
        *   `firmware-symlink`: Template filename to use for the /lib/firmware
            symlink to the firmware file in `/opt/google/touch`. The
            `/lib/firmware` part is assumed.
        *   `bcs-type` (optional): phandle pointing to the BCS node to use to obtain
                a tarfile containing the firmware.

        Template filenames may include the following fields, enclosed in
        `{...}` defined by the touch node: `vendor`, `pid`, `version` as well
        as `model` for the model name. The expansion / interpretation happens
        in cros_config. Other users should not attempt to implement this. The
        purpose is to avoid having to repeat the filename in each model that
        uses a particular manufacturer's touchscreen, since the naming
        convention is typically consistent for that manufacturer.

*   `models`: Sub-nodes of this define models supported by this board.

    *   `<model name>`: actual name of the model being defined, e.g. `reef` or
        `pyro`
        *   `arc` (optional): Contains arc++ configuration information
            *   `hw-features`: Script filename that configures the Arc++
                    hardware features (by probing or hard-coding) for a model.
            *   `build-properties` (optional): Contains information for the
                Android container system properties used by this model.
                Properties here are the same as in the family node above, with
                one addition to provide common values:
                *   `arc-properties-type`: Phandle pointing to a subnode of the
                    family arc build-properties configuration.
        *   `audio` (optional): Contains information about audio devices
                used by this model.
            *   `<audio_system>`: Contains information about a particular
                audio device used by this model. Valid values for the package
                name are:
                *   `main`: The main audio system

                For each of these:

                *   `audio-type`: Phandle pointing to a subnode of the family
                    audio configuration.

                All properties defined by the family subnode can be used here.
                Typically it is enough to define only `cras-config-dir`,
                `ucm-suffix` and `topology-name`. The rest are generally defined
                in terms of these, within the family configuration nodes.

        *   `brand-code`: (optional): Brand code of the model (also called RLZ
            code). See [list](go/chromeos-rlz) and
            [one-pager](gi/chromeos-rlz-onepager).

        *   `default` (optional): Indicates that all of the nodes and properties
            of this model should default to the same as another model. The value
            is a phandle pointing to the model. It is not possible to 'remove'
            nodes / properties defined by the other model. It is only possible
            to change properties or add new ones.
            Note: This is an experimental feature which will be evaluated in
            December 2017 to determine its usefulness versus the potential
            confusion it can cause.
        *   `thermal`(optional): Contains information about thermal properties
            and settings.
            *   `dptf-dv`: Filename of the .dv file containing DPTF (Dynamic
                Platform and Thermal Framework) settings, relative to the
                ebuild's FILESDIR.
        *   `touch` (optional): Contains information about touch devices such
            as touchscreens, touchpads, stylus.
            *   `present` (optional): Indicates whether this model has a
                touchscreen. This is used by the ARC++ system to pass
                information to Android, for example. Valid values are:
                *   `no`: This model does not have a touchscreen (default)
                *   `yes`: This model has a touchscreen
                *   `probe`: This model might have a touchscreen but we need to
                    probe at run-time to find out. This should ideally only be
                    needed on legacy devices which were not shipped with
                    unibuild.
            *   `probe-regex` (optional): Indicates the regular expression that
                should be used to match again device names in
                `sys/class/input/input*/name`. If the expression matches any
                device then the touchscreen is assumed to exist.
            *   `<device_type>` (optional): Contains information about touch
                firmware packages. Valid values for package_name are:
                * `stylus` - a pen-like device with a sensor on or behind the
                    display which together provide absolute positions with
                    respect to the display
                * `touchpad` - a touch surface separate from the display
                * `touchscreen` - a transparent touch surface on a display which
                    provides absolute positions with respect to the display

                You can use unit values (`touchscreen@0`, `touchscreen@1`) to
                allow multiple devices of the same type on a model.

                For each of these:

                *   `touch-type`: Phandle pointing to the `touch` node in the
                    Family configuration. This allows the vendor name and
                    default firmware file template to be defined.
                *   `pid`: Product ID string, as defined by the vendor.
                *   `version`: Version string, as defined by the vendor.
                *   `firmware-bin` (optional): Filename of firmware file. See
                    the Family `touch` node above for the format. If not
                    specified then the firmware-bin property from touch-type is
                    used.
                *   `firmware-symlink`: Filename of firmware file within
                    /lib/firmware on the device. See the Family `touch` node
                    above for the format.

        *   `wallpaper` (optional): base filename of the default wallpaper to
            show on this device. The base filename points `session_manager` to
            two files in the `/usr/share/chromeos-assets/wallpaper/<wallpaper>`
            directory: `/[filename]_[small|large].jpg`. If these files are
            missing or the property does not exist, "default" is used.
        *   `whitelabel` (optional): Sometimes models are so similar that we do
            not want to have separate settings. This happens in particular with
            'white-label' devices, where the same device is shipped by several
            OEMs under difference brands. This is a phandle pointing to another
            model whose configuration is shared. All settings (except for a very
            few exceptions) will then come from the shares node. Currently if
            this properly is used, then only the `firmware { key-id }`,
            `brand-code` and  `wallpaper` propertles can be provided. All other
            properties will come from the shared model.
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
                pins. At this time, only a phandle reference to a subnode of
                family/firmware is supported. There are no restrictions on
                the phandle target node naming. Note that this
                property cannot be provided if the model configuration is shared
                at the model level (the `whitelabel` property under
                `<model_name>`).
            *   `key-id` (optional): Unique ID that matches which key
                will be used in for firmware signing as part of vboot.
                For context, see go/cros-unibuild-signing
            *   `sig-id-in-customization-id` (optional): Indicates that this
                model cannot be decoded by the mapping table. Instead the model
                is stored in the VPD (Vital Product Data) region in the
                customization_id property. This allows us to determine the
                model to use in the factory during the finalization stage. Note
                that if the VPD is wiped then the model will be lost. This may
                mean that the device will revert back to a generic model, or
                may not work. It is not possible in general to test whether the
                model in the VPD is correct at run-time. We simply assume that
                it is. The advantage of using this property is that no hardware
                changes are needed to change one model into another. For example
                we can create 20 different whitelabel boards, all with the same
                hardware, just by changing the customization_id that is written
                into SPI flash.
        *   `powerd-prefs` (optional): Name of a subdirectory under the powerd
            model_specific prefs directory where model-specific prefs files are
            stored.
        *   `test-label` (optional): Test label applied to DUTs in the lab.
            In Autotest, this will be the model label.  By allowing an alternate
            label, different models can be shared for testing purposes.

### Example for reef

```
chromeos {
    family {
        audio {
            audio_type: audio-type {
                card = "bxtda7219max";
                volume = "cras-config/{cras-config-dir}/{card}";
                dsp-ini = "cras-config/{cras-config-dir}/dsp.ini";
                hifi-conf = "ucm-config/{card}.{ucm-suffix}/HiFi.conf";
                alsa-conf = "ucm-config/{card}.{ucm-suffix}/{card}.{ucm-suffix}.conf";
                topology-bin = "topology/5a98-reef-{topology-name}-8-tplg.bin";
            };
        };
        bcs {
            touch_bcs: touch-bcs {
                overlay = "overlay-reef-private";
                package = "chromeos-touch-firmware-reef";
                tarball = "chromeos-base/{package}/{package}-{ebuild-version}.tbz2";
                ebuild-version = "1.0-r9";
            };
        };
        firmware {
            script = "updater4.sh";
            shared: reef {
                bcs-overlay = "overlay-reef-private";
                main-image = "bcs://Reef.9042.50.0.tbz2";
                ec-image = "bcs://Reef-EC.9042.43.0.tbz2";
                extra = "${FILESDIR}/extra",
                    "${SYSROOT}/usr/sbin/ectool",
                    "bcs://Reef.something.tbz2";
                build-targets {
                    coreboot = "reef";
                    ec = "reef";
                    depthcharge = "reef";
                    libpayload = "reef";
                    cr50 = "cr50";
                };
            };
            pinned_version: sand {
                bcs-overlay = "overlay-reef-private";
                main-image = "bcs://Reef.9041.50.0.tbz2";
                ec-image = "bcs://Reef-EC.9041.43.0.tbz2";
                extra = "${FILESDIR}/extra",
                    "${SYSROOT}/usr/sbin/ectool",
                    "bcs://Reef.something.tbz2";
                build-targets {
                    coreboot = "reef";
                    ec = "reef";
                    depthcharge = "reef";
                    libpayload = "reef";
                };
            };
        };
        power {
          power_type: power-type {
            charging-ports = "CROS_USB_PD_CHARGER0 LEFT\nCROS_USB_PD_CHARGER1 RIGHT";
            low-battery-shutdown-percent = "4.0";
            power-supply-full-factor = "0.94";
            suspend-to-idle = "1";
          };
        };

        mapping {
            #address-cells = <1>;
            #size-cells = <0>;
            sku-map@0 {
                reg = <0>;
                platform-name = "Reef";
                smbios-name-match = "reef";
                /* This is an example! It does not match any real family */
                simple-sku-map = <
                   0 &basking
                   4 &reef_4
                   5 &reef_5
                   8 &electro>;
            };
            sku-map@1 {
                reg = <1>;
                platform-name = "Pyro";
                smbios-name-match = "pyro";
                single-sku = <&pyro>;
            };
            sku-map@2 {
                reg = <2>;
                platform-name = "Snappy";
                smbios-name-match = "snappy";
                single-sku = <&snappy>;
            };
            sku-map@3 {
                reg = <3>;
                platform-name = "Sand";
                smbios-name-match = "sand";
                single-sku = <&sand>;
            };
        };

        touch {
            elan_touchscreen: elan-touchscreen {
                bcs-type = <&touch_bcs>;
                vendor = "elan";
                firmware-bin = "{vendor}/{pid}_{version}.bin";
                firmware-symlink = "{vendor}ts_i2c_{pid}.bin";
            };
            elan_touchpad: elan-touchpad {
                bcs-type = <&touch_bcs>;
                vendor = "elan";
                firmware-bin = "{vendor}/{pid}_{version}.bin";
                firmware-symlink = "{vendor}_i2c_{pid}.bin";
            };
            wacom_stylus: wacom-stylus {
                /* This uses ${FILESDIR}, not BCS */
                vendor = "wacom";
                firmware-bin = "wacom/wacom_{version}.hex";
                firmware-symlink = "wacom_firmware_{model}.bin";
            };
            weida_touchscreen: weida-touchscreen {
                /* This uses ${FILESDIR}, not BCS */
                vendor = "weida";
                firmware-bin = "weida/{pid}_{version}_{date-code}.bin";
                firmware-symlink = "wdt87xx.bin";
            };
        };

        arc {
            build-properties {
                arc_properties_type: arc-properties-type {
                    device = "{product}_cheets";
                };
            };
        };
    };

    models {
        reef: reef {
            powerd-prefs = "reef";
            wallpaper = "seaside_life";
            brand-code = "ABCD";
            arc {
                hw-features = "reef/arc++/hardware_features";
                build-properties {
                    arc-properties-type = <&arc_properties_type>;
                    product = "robo";
                    oem = "Whyhub";
                    marketing-name = "{oem} Chromebook 23";
                    metrics-tag = "robo";
                };
            };
            firmware {
                shares = <&shared>;
                key-id = "REEF";
            };
            power {
              power-type = <&power_type>;
              set-wifi-transmit-power-for-tablet-mode = "1";
            };
            thermal {
                dptf-dv = "reef/dptf.dv";
            };
            submodels {
                reef_4: reef-touchscreen {
                    touch {
                        present = "yes";
                    };
                    audio {
                        main {
                            audio-type = <&audio_type>;
                            cras-config-dir = "front";
                            ucm-suffix = "front";
                            topology-name = "front";
                        };
                    };
                };
                reef_5: reef-notouch {
                    touch {
                        present = "no";
                    };
                    audio {
                        main {
                            audio-type = <&audio_type>;
                            cras-config-dir = "rear";
                            ucm-suffix = "rear";
                            topology-name = "rear";
                        };
                    };
                };
            };
            touch {
                present = "probe";
                probe-regex = "[Tt]ouchscreen\|WCOMNTN2";
                touchscreen {
                    touch-type = <&elan_touchscreen>;
                    pid = "0a97";
                    version = "1012";
                };
            };
        };

        pyro: pyro {
            powerd-prefs = "pyro_snappy";
            wallpaper = "alien_invasion";
            brand-code = "ABCE";
            audio {
                main {
                    audio-type = <&audio_type>;
                    cras-config-dir = "pyro";
                    ucm-suffix = "pyro";
                    topology-name = "pyro";
                };
            };
            firmware {
                bcs-overlay = "overlay-pyro-private";
                main-image = "bcs://Pyro.9042.41.0.tbz2";
                ec-image = "bcs://Pyro_EC.9042.41.0.tbz2";
                key-id = "PYRO";
                build-targets {
                    coreboot = "pyro";
                    ec = "pyro";
                    depthcharge = "pyro";
                    libpayload = "reef";
                };
            };
            power {
              power-type = <&power_type>;
            };
            thermal {
                dptf-dv = "pyro/dptf.dv";
            };
        };

        snappy: snappy {
            powerd-prefs = "pyro_snappy";
            wallpaper = "chocolate";
            brand-code = "ABCF";
            audio {
                main {
                    audio-type = <&audio_type>;
                    cras-config-dir = "snappy";
                    ucm-suffix = "snappy";
                    topology-name = "snappy";
                };
            };
            firmware {
                bcs-overlay = "overlay-snappy-private";
                main-image = "bcs://Snappy.9042.43.0.tbz2";
                ec-image = "bcs://Snappy_EC.9042.43.0.tbz2";
                key-id = "SNAPPY";
                build-targets {
                    coreboot = "snappy";
                    ec = "snappy";
                    depthcharge = "snappy";
                    libpayload = "reef";
                };
            };
            power {
              power-type = <&power_type>;
              keyboard-backlight-no-als-brightness = "10.0";
            };
        };

        basking: basking {
            powerd-prefs = "reef";
            test-label = "reef";
            wallpaper = "coffee";
            brand-code = "ABCG";
            firmware {
                shares = <&shared>;
                key-id = "BASKING";
            };
            power {
              power-type = <&power_type>;
            };
            touch {
                #address-cells = <1>;
                #size-cells = <0>;
                present = "yes";
                stylus {
                    touch-type = <&wacom_stylus>;
                    version = "4209";
                };
                touchpad {
                    touch-type = <&elan_touchpad>;
                    pid = "97.0";
                    version = "6.0";
                };
                touchscreen@0 {
                    reg = <0>;
                    touch-type = <&elan_touchscreen>;
                    pid = "3062";
                    version = "5602";
                };
                touchscreen@1 {
                    reg = <1>;
                    touch-type = <&weida_touchscreen>;
                    pid = "01017401";
                    version = "2082";
                    date-code = "0133c65b";
                };
            };
        };

        sand: sand {
            powerd-prefs = "reef";
            wallpaper = "coffee";
            brand-code = "ABCH";
            audio {
                main {
                    audio-type = <&audio_type>;
                    cras-config-dir = "sand";
                    ucm-suffix = "sand";
                    topology-name = "sand";
                };
            };
            firmware {
                shares = <&pinned_version>;
                key-id = "SAND";
            };
            power {
              power-type = <&power_type>;
            };
        };

        electro: electro {
            powerd-prefs = "reef";
            test-label = "reef";
            wallpaper = "coffee";
            brand-code = "ABCI";
            firmware {
                shares = <&pinned_version>;
                key-id = "ELECTRO";
                no-firmware;
            };
            power {
              power-type = <&power_type>;
            };
        };

        /* Whitelabel model */
        whitetip: whitetip {
            firmware {
                shares = <&shared>;
            };
        };

        whitetip1 {
            whitelabel = <&whitetip>;
            wallpaper = "shark";
            brand-code = "SHAR";
            firmware {
                key-id = "WHITELABEL1";
            };
        };

        whitetip2 {
            whitelabel = <&whitetip>;
            wallpaper = "more_shark";
            brand-code = "SHAQ";
            firmware {
                key-id = "WHITELABEL2";
            };
        };
        zt_whitelabel: zero-touch-whitelabel {
            firmware {
                sig-id-in-customization-id;
                shares = <&shared>;
            };
        };
        zt1 {
            whitelabel = <&zt_whitelabel>;
            firmware {
                key-id = "ZT1";
            };
        };
        zt2 {
            whitelabel = <&zt_whitelabel>;
            firmware {
                key-id = "ZT2";
            };
        };
    };
};
```
[](begin_definitions)

## CrOS Config Type Definitions (v2)
### model
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| audio | [audio](#audio) |  | True |  |
| brand-code | string | ```^[A-Z]{4}$``` | True | Brand code of the model (also called RLZ code). |
| firmware | [firmware](#firmware) |  | True |  |
| identity | [identity](#identity) |  | True |  |
| name | string | ```^[_a-zA-Z0-9]{3,}``` | True | Unique name for the given model. |
| powerd-prefs | string |  | True | Powerd config that should be used. |
| test-alias | string |  | True | Test alias (model) label that will be applied in Autotest and reported for test results. |
| touch | [touch](#touch) |  | False |  |

### audio
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| main | [main](#main) |  | True |  |

### main
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| card | string |  | True | Name of the card. |
| cras-config-dir | string | ```^(/[^/ ]*)+/?$``` | True | Full path to cras config: /etc/cras/{cras-config-subdir} (Auto-generated) |
| cras-config-subdir | string |  | False | Optional subdir for model specific configuration. |
| disable-profile | string |  | False | Optional --disable_profile parameter for CRAS deamon. |
| files | array - [files](#files) |  | True | All of the audio files configured. |
| topology-bin | string |  | False | Optional topology firmware filename. |
| ucm-suffix | string |  | False | Optional UCM suffix used to determine model specific config. |

### files
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| source | string |  | False | File source in the build structure. |
| target | string |  | False | Where the file will be installed on the image |

### firmware
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| bcs-overlay | string |  | True | BCS overlay path used to determine BCS file path for binary firmware downloads. |
| bcs-uris | array - string |  | False | Fully qualified paths to all of the BCS files for download |
| build-targets | [build-targets](#build-targets) |  | False |  |
| ec-image | string |  | True | Name of the file located in BCS under the respective bcs-overlay. |
| key-id | string | ```^[A-Z|_|0-9]*$``` | False | Key ID from the signer key set that is used to sign the given firmware image. |
| main-image | string |  | True | Name of the file located in BCS under the respective bcs-overlay. |
| main-rw-image | string |  | False | Name of the file located in BCS under the respective bcs-overlay. |

### build-targets
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| coreboot | string |  | False | Build target that will be considered dirty when building/testing locally. |
| cr50 | string |  | False | Build target that will be considered dirty when building/testing locally. |
| depthcharge | string |  | False | Build target that will be considered dirty when building/testing locally. |
| ec | string |  | False | Build target that will be considered dirty when building/testing locally. |
| libpayload | string |  | False | Build target that will be considered dirty when building/testing locally. |

### identity
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| customization-id | string | ```^[A-Z|_]*$``` | False | Customization ID set in the VPD during manufacturing. |
| sku-id | integer |  | True | SKU/Board strapping pins configured during board manufacturing. |
| smbios-name-match | string |  | False | Firmware name built into the firmware and reflected back out in the SMBIOS tables. |

### touch
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| present | string |  | True | Whether touch is present or needs to be probed for. |
| probe-regex | string |  | False | If probe is set, the regex used to look for touch. |
| stylus | [stylus](#stylus) |  | False |  |
| touchpad | [touchpad](#touchpad) |  | False |  |
| touchscreen | [touchscreen](#touchscreen) |  | False |  |
| touchscreen@0 | [touchscreen@0](#touchscreen@0) |  | False |  |
| touchscreen@1 | [touchscreen@1](#touchscreen@1) |  | False |  |

### stylus
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| ebuild-version | string |  | False | Tarfile version to download. This corresponds to the ebuild version prior to unibuild, but can be any suitable string. |
| firmware-bin | string |  | True | Path to the firmware binary for the touch firmware. |
| firmware-symlink | string |  | True | Name of the firmware symlink file. |
| overlay | string |  | False | Name of overlay to download from |
| package | string |  | False | Package subdirectory to download from |
| tarball | string |  | False | Template for tarball to download. This can maybe one day include {package} and {version}. |

### touchpad
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| ebuild-version | string |  | False | Tarfile version to download. This corresponds to the ebuild version prior to unibuild, but can be any suitable string. |
| firmware-bin | string |  | True | Path to the firmware binary for the touch firmware. |
| firmware-symlink | string |  | True | Name of the firmware symlink file. |
| overlay | string |  | False | Name of overlay to download from |
| package | string |  | False | Package subdirectory to download from |
| tarball | string |  | False | Template for tarball to download. This can maybe one day include {package} and {version}. |

### touchscreen
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| ebuild-version | string |  | False | Tarfile version to download. This corresponds to the ebuild version prior to unibuild, but can be any suitable string. |
| firmware-bin | string |  | True | Path to the firmware binary for the touch firmware. |
| firmware-symlink | string |  | True | Name of the firmware symlink file. |
| overlay | string |  | False | Name of overlay to download from |
| package | string |  | False | Package subdirectory to download from |
| tarball | string |  | False | Template for tarball to download. This can maybe one day include {package} and {version}. |

### touchscreen@0
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| ebuild-version | string |  | False | Tarfile version to download. This corresponds to the ebuild version prior to unibuild, but can be any suitable string. |
| firmware-bin | string |  | True | Path to the firmware binary for the touch firmware. |
| firmware-symlink | string |  | True | Name of the firmware symlink file. |
| overlay | string |  | False | Name of overlay to download from |
| package | string |  | False | Package subdirectory to download from |
| tarball | string |  | False | Template for tarball to download. This can maybe one day include {package} and {version}. |

### touchscreen@1
| Attribute | Type   | RegEx     | Required | Description |
| --------- | ------ | --------- | -------- | ----------- |
| ebuild-version | string |  | False | Tarfile version to download. This corresponds to the ebuild version prior to unibuild, but can be any suitable string. |
| firmware-bin | string |  | True | Path to the firmware binary for the touch firmware. |
| firmware-symlink | string |  | True | Name of the firmware symlink file. |
| overlay | string |  | False | Name of overlay to download from |
| package | string |  | False | Package subdirectory to download from |
| tarball | string |  | False | Template for tarball to download. This can maybe one day include {package} and {version}. |


[](end_definitions)
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


### Creating touch settings

To enable touch on a Chromebook you need to set up the touch firmware
correctly.

First, create a `touch` node in your family. Add to that subnodes for each
type of touch device you have, e.g. elan-touchpad, wacom-stylus. Each node
should specify the firmware-bin and firmware-symlink filename patterns for that
device.

Once you have done that you can add a `touch` node in your model. This should
reference the touch firmware node using the `touch-type` property. It should
also define things needed to identify the firmware, typically `pid` and
`version`.

Make sure that the ebuild which installs your BSP files (e.g.
`chromeos-bsp-coral-private`) calls the `install_touch_files` function from the
`cros-unibuild`. Check that your ebuild inherits `cros-unibuild` too.

If you are adding touch support for a new model in the family, take a look at
what other models have done.

To test your changes (e.g. for coral):

   emerge-coral chromeos-config-bsp chromeos-config chromeos-bsp-coral-private

You should see it install each of the touch files. If not, or you get an error,
check your configuration.

To test at run-time, build a new image and write it to your device. Check that
the firmware files are installed in /lib/firmware and loaded correctly by your
kernel driver.

## Adding and testing new properties

To introduce a new property, first add its definition to this file and to
cros_config_host/validate_config.py. Then to generate the new version of the
utility familiar with the new property run

  $ cros_workon --host start chromeos-config-host
  $ sudo emerge chromeos-config-host

Next add the new property and its value to the approritate dtsi file(s) in the
board's private overlay, in src/private-overlays. Either in

overlay-${BOARD}-private/chromeos-base/chromeos-config-bsp/files/<MODEL>/model.dtsi

if each model has its own configuration file or in

overlay-${BOARD}-private/chromeos-base/chromeos-config-bsp/files/model.dtsi

in case multiple models' configurations are described in a single file.

To compile the new database run

 $ cros_workon-${BOARD} start chromeos-config-bsp
 $ emerge-${BOARD} chromeos-config-bsp

and then to install the database into the board file system run

 $ emerge-${BOARD} chromeos-config

At this point the new compiled database is installed in
/build/${BOARD}/usr/share/chromeos-config/config.dtb.

To query your new item run the test command in the chroot:

 $ cros_config_host_py -c /build/${BOARD}/usr/share/chromeos-config/config.dtb \
    -m <MODEL> get </path/to/property> <property name>

for instance:

 $ cros_config_host_py -c /build/coral/usr/share/chromeos-config/config.dtb \
    -m robo360 get /firmware key-id
