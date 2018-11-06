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

3.  File Imports - File imports allow common snippets of YAML to be shared
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

### YAML Merging (multiple source YAML files)

There are various cases where it makes sense to manage YAML config across
multiple different repos in separate YAML files.

E.g.

* Permissions based control via git repo access to specific config
* Extending overlays for device customization (e.g. Moblab)

This is supported through cros_config_schema tool and is invoked as part of the
chromeos-config ebuild.

Using normal portage ebuilds/config, users can install as many YAML files as
they wish to be merged together into: /usr/share/chromeos-config/yaml

E.g.

* models.yaml
* models-private.yaml (private config overlaid on the public config)

These files are then merged together based on their lexicographic name order.

Merging of YAML files applies the following characteristics:

1. Order is important.  If two files supply the same config, the last file wins.
2. Identity is important.  Config is merged based on ONE OF the following:

   1. name - If the name attribute matches, the config is merged
   2. identity - Identity can be matched on all or some of the attributes.

For a detailed example of how merging works, see the following test files:

1. [test_merge_base.yaml](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/chromeos-config/libcros_config/test_merge_base.yaml) - First file passed to the merge routine.
2. [test_merge_overlay.yaml](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/chromeos-config/libcros_config/test_merge_overlay.yaml) - Second file passed (winner of any conflicts).
3. [test_merge.json](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/chromeos-config/libcros_config/test_merge.json) - Result generated from the merge.


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
| brand-code | string |  | False |  | Brand code of the model (also called RLZ code). |
| firmware | [firmware](#firmware) |  | True |  |  |
| firmware-signing | [firmware-signing](#firmware-signing) |  | False |  |  |
| hardware-properties | [hardware-properties](#hardware-properties) |  | False |  | Contains boolean flags for hardware properties of this board, for example if it's convertible, has a touchscreen, has a camera, etc. This information is used to auto-generate C code that is consumed by the EC build process in order to do run-time configuration. If a value is defined within a config file, but not for a specific model, that value will be assumed to be false for that model. All properties must be booleans. If non-boolean properties are desired, the generation code in cros_config_schema.py must be updated to support them. |
| identity | [identity](#identity) |  | False |  | Defines attributes that are used by cros_config to detect the identity of the platform and which corresponding config should be used. This tuple must either contain x86 properties only or ARM properties only. |
| modem | [modem](#modem) |  | False |  |  |
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
| name | string |  | False |  | This is a human-recognizable name used to refer to the firmware. It will be used when generating the shellball via firmware packer. Mainly, this is only for compatibility testing with device tree (since DT allowed firmwares to be named). |
| no-firmware | boolean |  | False |  | If present this indicates that this model has no firmware at present. This means that it will be omitted from the firmware updater (chromeos-firmware- ebuild) and it will not be included in the signer instructions file sent to the signer. This option is often useful when a model is first added, since it may not have firmware at that point. |
| pd-image | string |  | False |  | Name of the file located in BCS under the respective bcs-overlay. |
| tools | array - string |  | False |  | A list of additional tools which should be packed into the firmware update shellball. This is only needed if this model needs to run a special tool to do the firmware update. |

### build-targets
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| base | string |  | False |  | Build target of the base EC firmware for a detachable device, that will be considered dirty when building/testing |
| coreboot | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| cr50 | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| depthcharge | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| ec | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| ec_extras | array - string |  | False |  | Extra EC build targets to build within chromeos-ec. |
| libpayload | string |  | False |  | Build target that will be considered dirty when building/testing locally. |
| u-boot | string |  | False |  | Build target that will be considered dirty when building/testing locally. |

### firmware-signing
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| key-id | string |  | True |  | Key ID from the signer key set that is used to sign the given firmware image. |
| sig-id-in-customization-id | boolean |  | False |  | Indicates that this model cannot be decoded by the mapping table. Instead the model is stored in the VPD (Vital Product Data) region in the customization_id property. This allows us to determine the model to use in the factory during the finalization stage. Note that if the VPD is wiped then the model will be lost. This may mean that the device will revert back to a generic model, or may not work. It is not possible in general to test whether the model in the VPD is correct at run-time. We simply assume that it is. The advantage of using this property is that no hardware changes are needed to change one model into another. For example we can create 20 different whitelabel boards, all with the same hardware, just by changing the customization_id that is written into SPI flash. |
| signature-id | string |  | True |  | ID used to generate keys/keyblocks in the firmware signing output.  This is also the value provided to mosys platform signature for the updater4.sh script. |

### hardware-properties
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| has-base-accelerometer | boolean |  | False |  | Is there an accelerometer in the base of the device. |
| has-base-gyroscope | boolean |  | False |  | Is there a gyroscope in the base of the device. |
| has-fingerprint-sensor | boolean |  | False |  | Is there a fingerprint sensor on the device. |
| has-lid-accelerometer | boolean |  | False |  | Is there an accelerometer in the lid of the device. |
| is-lid-convertible | boolean |  | False |  | Can the lid be rotated 360 degrees. |

### identity
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| customization-id | string |  | False | x86 | 'customization_id' value set in the VPD for non-unibuild Zergs and Whitelabels. Deprecated for use in new products since 2017/07/26. |
| platform-name | string |  | False | x86 | Defines the name that is reported by 'mosys platform name' This is typically the reference design name with the first letter capitalized |
| sku-id | integer |  | False | x86 | SKU/Board strapping pins configured during board manufacturing. |
| smbios-name-match | string |  | False | x86 | [x86] Firmware name built into the firmware and reflected back out in the SMBIOS tables. |
| whitelabel-tag | string |  | False | x86 | 'whitelabel_tag' value set in the VPD, to add Whitelabel branding over an unbranded base model. |
| customization-id | string |  | False | ARM | 'customization_id' value set in the VPD for non-unibuild Zergs and Whitelabels. Deprecated for use in new products since 2017/07/26. |
| device-tree-compatible-match | string |  | False | ARM | [ARM] String pattern (partial) that is matched against the contents of /proc/device-tree/compatible on ARM devices. |
| platform-name | string |  | False | ARM | Defines the name that is reported by 'mosys platform name' This is typically the reference design name with the first letter capitalized |
| sku-id | integer |  | False | ARM | SKU/Board strapping pins configured during board manufacturing. |
| whitelabel-tag | string |  | False | ARM | 'whitelabel_tag' value set in the VPD, to add Whitelabel branding over an unbranded base model. |

### modem
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| firmware-variant | string |  | False |  | Variant of the modem firmware to be used. This value is read by modemfwd to match against the variant field of a firmware entry in a firmware manifest. In most cases, we simply use the model name as the value. |

### power
| Attribute | Type   | RegEx     | Required | Oneof Group |  Description |
| --------- | ------ | --------- | -------- | ----------- |  ----------- |
| [ANY] | N/A | N/A | N/A| N/A | This type allows additional properties not governed by the schema. See the type description for details on these additional properties.|
| touchpad-wakeup | string | ```^[01]$``` | False |  | Enable (1) or disable (0) wake from touchpad. |

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
