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
on the target, and cros_config_host for use on the host/during building. See the
--help for each tool respectively for help on usage.

## Debugging

libcros_config will emit a lot of debugging log messages if you set the
CROS_CONFIG_DEBUG environment variable to a non-empty value before calling into
the library.

## Config Schema

Chrome OS config is now YAML based. If you're still using the Flat Device Tree
(FDT) based implementation, see the v1 schema description below.

The following components make up the YAML based chromeos-config support:

### YAML Source

The YAML source is designed for human maintainability. It allows for easy config
sharing across many different devices (via anchors).

For background on YAML, see: [Learn YAML in 10
minutes](https://learnxinyminutes.com/docs/yaml/)

The source is generally located at:
overlay-${BOARD}/chromeos-base/chromeos-config-bsp/files/model.yaml

Beyond the normal features of YAML, there are a few custom features supported
that allow for even better re-use and expressiveness in the YAML config.

1.  Templating - Templating allows config to be shared by letting callers
    reference variables in the config, which are then evaluated on a per
    device/sku/product basis.

    The basic syntax is:

    ```yaml
    some-element: "{{some-template-variable}}"
    ```

    Valid template variables are any YAML elements that are currently in scope.
    When generating config, scope is evaluated in the following order:

    1.  sku
    2.  config (this is recursive ... any variable at any level can be
        referenced)
    3.  device
    4.  product

    This order allows shared anchors to define default variables that are then
    optionally overridden by either the device or product scope.

2.  Local Variables - These are variables that are only used for templating and
    are dropped in the final JSON output. Variables starting with '$' are
    considered local and are ignored after template evaluation. The basic syntax
    is:

    ```yaml
    config:
      $some-local-variable: "some-value"
      some-element: "{{$some-local-variable}}"
    ```

    This supports the following:

    1.  Defining local variables that are re-used in multiple places (e.g. file
        paths).
    2.  Re-using common config (e.g. 'identity') where the variable value isn't
        known until the device/product/sku variables come into scope (e.g.
        $sku-id).

2.  File Imports - File imports allow common snippets of YAML to be shared
    across multiple different implementations.  File importing works the same as
    if the YAML files were cat'd together and then evaluated.  File importing is
    recursive also, so it will support importing files that import other files.
    Import paths must be relative to the file that specifies the import.

    ```yaml
    imports:
      - "some_common_import_file.yaml"
      - "../common/some_other_common_import_file.yaml"
    ```

The following provides a simple example of a config using both core YAML
features and the custom features described above.

```yaml
common_config: &common_config
  name: "{{$device-name}}"
  brand-code: "{{$brand-code}}"
  identity:
    platform-name: "SomePlatform"
    smbios-name-match: "SomePlatform"
    sku-id: "{{$sku-id}}"
  firmware-signing:
    key-id: "{{$key-id}}"
    signature-id: "{{name}}"
chromeos:
  devices:
    - $device-name: "SomeDevice"
      products:
        - $brand-code: "YYYY"
          $key-id: "SOME-KEY-ID"
      skus:
        - $sku-id: 0
          config:
            <<: *common_config
            wallpaper: "some-wallpaper"
        - $sku-id: 1
          config: *common_config
```

When this YAML is evaluated, it will fully expand out as the following:

```yaml
chromeos:
  models:
    - name: "SomeDevice"
      brand-code: "YYYY"
      identity:
        platform-name: "SomePlatform"
        smbios-name-match: "SomePlatform"
        sku-id: 0
      firmware-signing:
        key-id: "SOME-KEY-ID"
        signature-id: "SomeDevice"
      wallpaper: "some-wallpaper"
    - name: "SomeDevice"
      brand-code: "YYYY"
      identity:
        platform-name: "SomePlatform"
        smbios-name-match: "SomePlatform"
        sku-id: 1
      firmware-signing:
        key-id: "SOME-KEY-ID"
        signature-id: "SomeDevice"
```

### YAML Transform (to JSON)

In addition to the templating evaluation discussed above, the YAML is converted
to JSON before it's actually used in chromeos-config. This fully
evaluated/de-normalized form accomplishes a couple things:

1.  It provides a great diffable format so it's obvious what changes are
    actually applied if, for example, a shared config element was changed.
2.  It keeps the consumer code very simple (host and runtime). The code just
    matches the identity attributes and then uses the respective config. It
    never has to care about re-use or config sharing.

The transform algorithm works as follows:

* FOREACH device in chromeos/devices
    * FOREACH product in device/products
        * FOREACH sku in device/skus
            * sku varibles are put into scope
            * config variables are put into scope
            * device variables are put into scope
            * product variables are put into scope
            * with sku/config
                * config template variables are evaluated
                * the config contents are captured and stored in the resulting json

Based on this algorithm, some key points are:

* Only 'sku/config' actually lands in the JSON output. All other YAML structure
  supports the generation of the sku/config payloads.
* 'product' generally defines identity/branding information only. The main
  reason multiple products are supported is for the whitelabel case.

### Schema Validation

The config is evaluated against a http://json-schema.org/ schema located at:
chromeos-config/cros_config_host/cros_config_schema.yaml

NOTE: The schema is managed in YAML because it's easier to edit than JSON.

Only the transformed JSON is actually evaluated against the schema. Authors can
do whatever makes sense in the YAML (from a sharing perspective) as long as it
generates compliant JSON that passes the schema validation.

The schema documentation is auto-generated (and put into this README.md file)
via: python -m cros_config_host.generate_schema_doc -o README.md

The schema definition is below:

[](begin_definitions)

## CrOS Config Type Definitions (v2)
### model
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| arc | [arc](#arc) |  | False |  |  |
| audio | [audio](#audio) |  | False |  |  |
| brand-code | string | ```^[A-Z]{4}$``` | False |  | Brand code of the model (also called RLZ code). |
| firmware | [firmware](#firmware) |  | True |  |  |
| firmware-signing | [firmware-signing](#firmware-signing) |  | False |  |  |
| identity | [identity](#identity) |  | False |  | Defines attributes that are used by cros_config to detect the identity of the platform and which corresponding config should be used. This tuple must either contain x86 properties only or ARM properties only. |
| name | string | ```^[_a-zA-Z0-9]{3,}``` | True |  | Unique name for the given model. |
| oem-id | string | ```[0-9]+``` | False |  | Some projects store SKU ID, OEM ID and Board Revision in an EEPROM and only SKU ID can be updated in the factory and RMA flow but others should be pre-flashed in the chip level. In this case, we would like to validate whether oem-id here from the updated SKU ID matches the one in the EEPROM so we can prevent this device from being updated to another OEM's devices.  |
| power | [power](#power) |  | False |  | WARNING -- This config contains unvalidated settings, which is not a correct usage pattern, but this will be used in the interim until a longer term solution can be put in place where the overall schema can be single sourced (for the YAML and C++ that uses it); likely though some type of code generation. SUMMARY -- Contains power_manager device settings.  This is the new mechanism used in lieu of the previous file based implementation (via powerd-prefs). Power manager will first check for a property in this config, else it will revert to the file based mechanism (via the powerd-prefs setting). This provides more flexibility in sharing power settings across different devices that share the same build overlay. Any property can be overridden from - src/platform2/power_manager/default_prefs or src/platform2/power_manager/optional_prefs For details about each setting property, see - src/platform2/power_manager/common/power_constants.h For examples on setting these properties (including multiline examples), see the power config example in libcros_config/test.yaml |
| powerd-prefs | string |  | False |  | Powerd config that should be used. |
| test-label | string |  | False |  | Test alias (model) label that will be applied in Autotest and reported for test results. |
| thermal | [thermal](#thermal) |  | False |  |  |
| touch | [touch](#touch) |  | False |  |  |
| ui | [ui](#ui) |  | False |  |  |
| wallpaper | string |  | False |  | Base filename of the default wallpaper to show on this device. |

### arc
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| build-properties | [build-properties](#build-properties) |  | False |  |  |
| files | array - [files](#files) |  | False |  |  |

### build-properties
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| device | string |  | False |  | Device name to report in 'ro.product.device'. This is often '{product}_cheets' but it can be something else if desired. |
| first-api-level | string |  | False |  | The first Android API level that this model shipped with.  |
| marketing-name | string |  | False |  | Name of this model as it is called in the market, reported in 'ro.product.model'. This often starts with '{oem}'. |
| metrics-tag | string |  | False |  | Tag to use to track metrics for this model. The tag can be shared across many devices if desired, but this will result in larger granularity for metrics reporting.  Ideally the metrics system should support collation of metrics with different tags into groups, but if this is not supported, this tag can be used to achieve the same end.  This is reported in 'ro.product.metrics.tag'. |
| oem | string |  | False |  | Original Equipment Manufacturer for this model. This generally means the OEM name printed on the device. |
| product | string |  | False |  | Product name to report in 'ro.product.name'. This may be the device name, or it can be something else, to allow several devices to be grouped into one product. |

### files
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| destination | string |  | False |  | Installation path for the file on the system image. |
| source | string |  | False |  | Source of the file relative to the build system. |

### audio
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| main | [main](#main) |  | True |  |  |

### main
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| cras-config-dir | string |  | True |  | Subdirectory for model-specific configuration. |
| disable-profile | string |  | False |  | Optional --disable_profile parameter for CRAS deamon. |
| files | array - [files](#files) |  | False |  |  |
| ucm-suffix | string |  | False |  | Optional UCM suffix used to determine model specific config. |

### files
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| destination | string |  | False |  | Installation path for the file on the system image. |
| source | string |  | False |  | Source of the file relative to the build system. |

### firmware
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| bcs-overlay | string |  | False |  | BCS overlay path used to determine BCS file path for binary firmware downloads. |
| build-targets | [build-targets](#build-targets) |  | False |  |  |
| ec-image | string |  | False |  | Name of the file located in BCS under the respective bcs-overlay. |
| extra | array - string |  | False |  | A list of extra files or directories needed to update firmware, each being a string filename. Any filename is supported. If it starts with `bcs://` then it is read from BCS as with main-image above. But normally it is a path. A typical example is `${FILESDIR}/extra` which means that the `extra` diectory is copied from the firmware ebuild's `files/extra` directory. Full paths can be provided, e.g. `${SYSROOT}/usr/bin/ectool`. If a directory is provided, its contents are copied (subdirectories are not supported). This mirrors the functionality of `CROS_FIRMWARE_EXTRA_LIST`. |
| key-id | string |  | False |  | Key ID from the signer key set that is used to sign the given firmware image. |
| main-image | string |  | False |  | Name of the file located in BCS under the respective bcs-overlay. |
| main-rw-image | string |  | False |  | Name of the file located in BCS under the respective bcs-overlay. |
| no-firmware | boolean |  | False |  | If present this indicates that this model has no firmware at present. This means that it will be omitted from the firmware updater (chromeos-firmware- ebuild) and it will not be included in the signer instructions file sent to the signer. This option is often useful when a model is first added, since it may not have firmware at that point. |
| pd-image | string |  | False |  | Name of the file located in BCS under the respective bcs-overlay. |
| tools | array - string |  | False |  | A list of additional tools which should be packed into the firmware update shellball. This is only needed if this model needs to run a special tool to do the firmware update. |

### build-targets
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| coreboot | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| cr50 | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| depthcharge | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| ec | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| libpayload | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| u-boot | string |  | False |  | Build target that will be considered dirty when building/testing locally. |

### firmware-signing
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| key-id | string |  | True |  | Key ID from the signer key set that is used to sign the given firmware image. |
| sig-id-in-customization-id | boolean |  | False |  | Indicates that this model cannot be decoded by the mapping table. Instead the model is stored in the VPD (Vital Product Data) region in the customization_id property. This allows us to determine the model to use in the factory during the finalization stage. Note that if the VPD is wiped then the model will be lost. This may mean that the device will revert back to a generic model, or may not work. It is not possible in general to test whether the model in the VPD is correct at run-time. We simply assume that it is. The advantage of using this property is that no hardware changes are needed to change one model into another. For example we can create 20 different whitelabel boards, all with the same hardware, just by changing the customization_id that is written into SPI flash. |
| signature-id | string |  | True |  | ID used to generate keys/keyblocks in the firmware signing output.  This is also the value provided to mosys platform signature for the updater4.sh script. |

### identity
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| customization-id | string |  | False | x86 | 'customization-id' value set in the VPD for Zergs and older Whitelabels. |
| platform-name | string |  | False | x86 | Defines the name that is reported by 'mosys platform name' This is typically the reference design name with the first letter capitalized |
| sku-id | integer |  | False | x86 | [x86] SKU/Board strapping pins configured during board manufacturing. |
| smbios-name-match | string |  | False | x86 | [x86] Firmware name built into the firmware and reflected back out in the SMBIOS tables. |
| whitelabel-tag | string |  | False | x86 | 'whitelabel-tag' value set in the VPD for Whitelabels. |
| customization-id | string |  | False | ARM | 'customization-id' value set in the VPD for Zergs and older Whitelabels. |
| device-tree-compatible-match | string |  | False | ARM | [ARM] String pattern (partial) that is matched against the contents of /proc/device-tree/compatible on ARM devices. |
| platform-name | string |  | False | ARM | Defines the name that is reported by 'mosys platform name' This is typically the reference design name with the first letter capitalized |
| whitelabel-tag | string |  | False | ARM | 'whitelabel-tag' value set in the VPD for Whitelabels. |

### power
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| [ANY] | N/A | N/A | N/A| N/A | This type allows additional properties not governed by the schema. See the type description for details on these additional properties.|

### thermal
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| dptf-dv | string |  | False |  | System image path to the .dv file containing DPTF (Dynamic Platform and Thermal Framework) settings. |
| files | array - [files](#files) |  | True |  |  |

### files
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| destination | string |  | False |  | Installation path for the file on the system image. |
| source | string |  | False |  | Source of the file relative to the build system. |

### touch
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| files | array - [files](#files) |  | False |  |  |
| present | string |  | False |  | Whether touch is present or needs to be probed for. |
| probe-regex | string |  | False |  | If probe is set, the regex used to look for touch. |

### files
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| destination | string |  | False |  | Installation path for the file on the system image. |
| source | string |  | False |  | Source of the file relative to the build system ${FILESDIR} |
| symlink | string |  | False |  | Symlink file that will be installed pointing to the destination. |

### ui
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| power-button | [power-button](#power-button) |  | False |  |  |

### power-button
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| edge | string |  | False |  |  |
| position | string |  | False |  |  |


[](end_definitions)

## Usage Instructions

## Adding and testing new properties

Before starting, cros_workon the following:

*   cros_workon --host start chromeos-config-host
*   cros_workon --board=BOARD start chromeos-config-bsp chromeos-config

To introduce a new property, first add its definition to the schema:

*   chromeos-config/cros_config_host/cros_config_schema.yaml

Then update the README.md automatically via (unit tests will check this):

*   python -m cros_config_host.generate_schema_doc -o README.md

To install the updated schema, run:

*   FEATURES=test sudo -E emerge chromeos-config-host

To use the new property, update your respective YAML source file. E.g.
overlay-${BOARD}-private/chromeos-base/chromeos-config-bsp/files/model.yaml

To install the changes, run:

*   emerge-${BOARD} chromeos-config-bsp chromeos-config

At this point the updated config is located at:

*   /build/${BOARD}/usr/share/chromeos-config/yaml/config.yaml

To query your new item run the test command in the chroot:

*   cros_config_host -c
    /build/${BOARD}/usr/share/chromeos-config/yaml/config.yaml -m <MODEL> get
    </path/to/property> <property name>

For instance:

*   cros_config_host -c /build/coral/usr/share/chromeos-config/yaml/config.yaml
    -m robo360 get /firmware key-id
*   cros_config_host -c /build/coral/usr/share/chromeos-config/yaml/config.yaml
    get-models

## Device Tree - Schema (v1)

The 2017 unibuild devices were launched under version 1 for chromeos-config. For
these cases, the v1 schema (below) is still used; however, all new devices
should be using the v2 schema (YAML based).

We plan to migrate all overlays to v2 as soon as possible. Please do not create
any new device tree based configs or update the device tree implementation with
features for overlays that have already been migrated.

*   `family`: Provides family-level configuration settings, which apply to all
    models in the family.

    *   `audio` (optional): Contains information about audio devices used by
        this family. Each subnode is defined as a phandle that can be referenced
        from the model-specific configuration using the `audio-type` property.
        *   `<audio-type>`: Node containing the audio config for one device
            type. All filenames referenced are in relation to the ${FILERDIR}
            directory of the ebuild containing them.
            *   `cras-config-dir`: Directory to pass to cras for the location of
                its config files
            *   `ucm-suffix`: Internal UCM suffix to pass to cras
            *   `topology-name` (optional): Name of the topology firmware to use
            *   `card`: Name of the audio 'card'
            *   `volume`: Template filename of volume curve file
            *   `dsp-ini`: Template filename of dsp.ini file
            *   `hifi-conf`: Template filename of the HiFi.conf file
            *   `alsa-conf`: Template filename of the card configuration file
            *   `topology-bin` (optional): Template filename of the topology
                firmware file Template filenames may include the following
                fields, enclosed in `{...}` defined by the audio node: `card`,
                `cras-config-dir`, `topology-name`, `ucm-suffix` as well as
                `model` for the model name. The expansion / interpretation
                happens in cros_config_host. Other users should not attempt to
                implement this. The purpose is to avoid having to repeat the
                filename in each model that uses a particular manufacturer's
                card, since the naming convention is typically consistent for
                that manufacturer.
    *   `arc` (optional): Contains information for the Android container used by
        this family.

        *   `build-properties` (optional): Contains information that will be set
            into the Android property system inside the container. Each subnode
            is defined as a phandle that can be referenced from the
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
                    with `{oem}`.
                *   `metrics-tag`: Tag to use to track metrics for this model.
                    The tag can be shared across many models if desired, but
                    this will result in larger granularity for metrics
                    reporting. Ideally the metrics system should support
                    collation of metrics with different tags into groups, but if
                    this is not supported, this tag can be used to achieve the
                    same end. This is reported in `ro.product.metrics.tag`.
                *   `first-api-level`: The first [Android API
                    level](https://source.android.com/setup/build-numbers) that
                    this model shipped with.

    *   `power` (optional): Contains information about power devices used by
        this family. Each subnode is defined as a phandle that can be referenced
        from the model-specific configuration using the `power-type` property.

        *   `<power-type>`: Node containing the power config for one device
            type. Each property corresponds to a power_manager preference, more
            completely documented in
            [power_manager](https://cs.corp.google.com/chromeos_public/src/platform2/power_manager/common/power_constants.h).
            *   `charging-ports`: String describing charging port positions.
            *   `keyboard-backlight-no-als-brightness`: Initial brightness for
                the keyboard backlight for systems without ambient light
                sensors, in the range [0.0, 100.0].
            *   `low-battery-shutdown-percent`: Battery percentage threshold at
                which the system should shut down automatically, in the range
                [0.0, 100.0].
            *   `power-supply-full-factor`: Fraction of the battery's total
                charge at which it should be reported as full in the UI, in the
                range (0.0, 1.0].
            *   `set-wifi-transmit-power-for-tablet-mode`: If true (1), update
                wifi transmit power when in tablet vs. clamshell mode.
            *   `suspend-to-idle`: If true (1), suspend to idle by writing
                freeze to /sys/power/state.

    *   `bcs` (optional): Provides a set of BCS (Binary Cloud Storage) sources
        which can be used to download files needed by the build system. Each
        subnode is defined as a phandle that can be referenced from a node which
        needs access to BCS.

        *   `<bcs-type>`: Node containing information about one BCS tarfile:
            *   `overlay`: Name of overlay to download from
            *   `package`: Package subdirectory to download from
            *   `ebuild-version`: Tarfile version to download. This corresponds
                to the ebuild version prior to unibuild, but can be any suitable
                string.
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
        particular device. There can be any number of mappings. At present only
        a `sku-map` is allowed.

        *   `sku-map`: Provides a mapping from SKU ID to model/sub-model. One of
            `simple-sku-map` or `single-sku` must be provided.
            `smbios-name-match` is needed only if the family supports models
            which have SKU ID conflicts and needs the SMBIOS name to
            disambiguate them. This is common when migrating legacy boards to
            unified builds, but may also occur if the SKU ID mapping is not used
            for some reason.
            *   `platform-name`: Indicates the platform name for this platform.
                This is reported by 'mosys platform name'. It is typically the
                family name with the first letter capitalized.
            *   `smbios-name-match` (optional) Indicates the smbios name that
                this table mapping relates to. This map will be ignored on
                models which don't have a matching smbios name.
            *   `simple-sku-map` (optional): Provides a simple mapping from SKU
                (an integer value) to model / sub-model. Each entry consists of
                a sku value (typically 0-255) and a phandle pointing to the
                model or sub-model.
            *   `single-sku` (optional): Used in cases where only a single model
                is supported by this mapping. In other words, if the SMBIOS name
                matches, this is the model to use. The value is a phandle
                pointing to the model (it cannot point to a sub-model).

    *   `touch` (optional): Contains information about touch devices used by
        this family. Each node is defined as a Phandle that can be referenced
        from the model-specific configuration using the `touch-type` property.

        *   `vendor`: Name of vendor.
        *   `firmware-bin`: Template filename to use for vendor firmware binary.
            The file is installed into `/opt/google/touch`.
        *   `firmware-symlink`: Template filename to use for the /lib/firmware
            symlink to the firmware file in `/opt/google/touch`. The
            `/lib/firmware` part is assumed.
        *   `bcs-type` (optional): phandle pointing to the BCS node to use to
            obtain a tarfile containing the firmware.

        Template filenames may include the following fields, enclosed in `{...}`
        defined by the touch node: `vendor`, `pid`, `version` as well as `model`
        for the model name. The expansion / interpretation happens in
        cros_config. Other users should not attempt to implement this. The
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
        *   `audio` (optional): Contains information about audio devices used by
            this model.

            *   `<audio_system>`: Contains information about a particular audio
                device used by this model. Valid values for the package name
                are:

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
            to change properties or add new ones. Note: This is an experimental
            feature which will be evaluated in December 2017 to determine its
            usefulness versus the potential confusion it can cause.

        *   `thermal`(optional): Contains information about thermal properties
            and settings.

            *   `dptf-dv`: Filename of the .dv file containing DPTF (Dynamic
                Platform and Thermal Framework) settings, relative to the
                ebuild's FILESDIR.

        *   `touch` (optional): Contains information about touch devices such as
            touchscreens, touchpads, stylus.

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

                *   `stylus` - a pen-like device with a sensor on or behind the
                    display which together provide absolute positions with
                    respect to the display
                *   `touchpad` - a touch surface separate from the display
                *   `touchscreen` - a transparent touch surface on a display
                    which provides absolute positions with respect to the
                    display

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
            `brand-code` and `wallpaper` propertles can be provided. All other
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
                family/firmware is supported. There are no restrictions on the
                phandle target node naming. Note that this property cannot be
                provided if the model configuration is shared at the model level
                (the `whitelabel` property under `<model_name>`).
            *   `key-id` (optional): Unique ID that matches which key will be
                used in for firmware signing as part of vboot. For context, see
                go/cros-unibuild-signing
            *   `sig-id-in-customization-id` (optional): Indicates that this
                model cannot be decoded by the mapping table. Instead the model
                is stored in the VPD (Vital Product Data) region in the
                customization_id property. This allows us to determine the model
                to use in the factory during the finalization stage. Note that
                if the VPD is wiped then the model will be lost. This may mean
                that the device will revert back to a generic model, or may not
                work. It is not possible in general to test whether the model in
                the VPD is correct at run-time. We simply assume that it is. The
                advantage of using this property is that no hardware changes are
                needed to change one model into another. For example we can
                create 20 different whitelabel boards, all with the same
                hardware, just by changing the customization_id that is written
                into SPI flash.

        *   `powerd-prefs` (optional): Name of a subdirectory under the powerd
            model_specific prefs directory where model-specific prefs files are
            stored.

        *   `test-label` (optional): Test label applied to DUTs in the lab. In
            Autotest, this will be the model label. By allowing an alternate
            label, different models can be shared for testing purposes.

        *   `ui` (optional): Config related to operation of the UI on this
            model.

            *   `power-button` (optional): Defines the position on the screen
                where the power-button menu appears after a long press of the
                power button on a tablet device. The position is defined
                according to the 'landscape-primary' orientation, so that if the
                device is rotated, the button position on the screen will follow
                the rotation.

                *   `edge`: Indicates which edge the power button is anchored
                    to. Can be "left", "right", "top", "bottom". For example,
                    "left" means that the menu will appear on the left side of
                    the screen, with the distance along that edge (top to
                    bottom) defined by the next property.
                *   `position': Indicates the position of the menu along that
                    edge, as a fraction, measured from the top or left of the
                    screen. For example, "0.3" means that the menu will be 30%
                    of the way from the origin (which is the left or top of the
                    screen).

        *   `oem-id` (optional): Some projects store SKU ID, OEM ID and Board
            Revision in an EEPROM and only SKU ID can be updated in the factory
            and RMA flow but others should be pre-flashed in the chip level. In
            this case, we would like to validate whether oem-id here from the
            updated SKU ID matches the one in the EEPROM so we can prevent this
            device from being updated to another OEM's models.
