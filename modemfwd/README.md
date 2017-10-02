# Modem Firmware Daemon

This daemon abstracts out the common portions of updating modem firmware, i.e.
deciding whether there is currently newer firmware available and getting the
modem ready to be flashed with new firmware.

## Modem-specific program API

In order to enforce a process boundary between the open-source `modemfwd` and
potentially non-open-source modem firmware updaters, we farm out steps that
require modem-specific knowledge to different programs. `modemfwd` will call
into these programs with different flags to request different services. These
flags are declared in the [system API] repo.

* `--prepare_to_flash`: put the modem into firmware download mode
* `--flash_main_fw=<file>`
* `--get_carrier_fw_info`: get a string name for the carrier that is supported
  by the currently installed carrier firmware, and its version (if there is any)
* `--flash_carrier_fw=<file>`
* `--reboot`

`--get_carrier_fw_info` should return the carrier name on one line and the
version on the next:

```
$ modem_program --get_carrier_fw_info
Carrier
10.20.30.40
```

All commands should return 0 on success and something non-zero on failure.
`modemfwd` will look for these binaries in the directory passed as the
`--helper_directory` argument on the command line.

## Helper and firmawre directory structure

The protos defined in `helper_manifest.proto` and `firmware_manifest.proto`
define manifests that should be provided in the helper and firmware directories
so modemfwd can figure out what devices, carriers, etc. the contents can be
used with.

[system API]: https://chromium.googlesource.com/chromiumos/platform/system_api/+/master/switches/modemfwd_switches.h
