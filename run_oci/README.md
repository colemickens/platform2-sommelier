# `run_oci`

## Overview

`run_oci` is a minimalistic container runtime that is (mostly) compatible with
the [OCI runtime spec](https://github.com/opencontainers/runtime-spec).

## Chrome OS extensions

The OCI runtime spec allows implementations to add additional properties for
[extensibility](https://github.com/opencontainers/runtime-spec/blob/master/config.md#extensibility).

Chrome OS adds the following extensions:

### Pre-chroot hooks

There are some bind-mounts that cannot be specified in the config file, since
the source paths for them are not fixed (e.g. the user's cryptohome path), or
can be enabled dynamically at runtime depending on [Chrome
Variations](https://www.google.com/chrome/browser/privacy/whitepaper.html#variations).

During the container setup in Chrome OS, there is a small window of time when
the container's mount namespace is completely set up, but
[chroot(2)](http://man7.org/linux/man-pages/man2/chroot.2.html) has not been yet
called, so bind mounts that cross the chroot boundary can still be performed.

The
[**`hooks`**](https://github.com/opencontainers/runtime-spec/blob/master/config.md#posix-platform-hooks)
object has been extended to also contain the following:

* **`prechroot`**: *(array of objects, OPTIONAL)* - is an array of pre-chroot
  hooks. Entries in the array have the same schema as pre-start entries, and are
  run in the outer namespace after all the entries in [**`mounts`**](https://github.com/opencontainers/runtime-spec/blob/master/config.md#mounts)
  have been mounted, but before chroot(2) has been invoked.

#### Example (Chrome OS)

    {
        "hooks": {
            "prechroot": [
                {
                    "path": "/usr/sbin/arc-setup",
                    "args": ["arc-setup", "--pre-chroot"]
                }
            ]
        }
    }

### Linux device node dynamic minor numbers

Device nodes that have well-known major/minor numbers are normally added to the
[**`devices`**](https://github.com/opencontainers/runtime-spec/blob/master/config-linux.md#devices)
array, whereas device nodes that have dynamic major/minor numbers are typically
bind-mounted. Android running in Chrome OS needs to have device node files
created in the container rather than bind-mounted, since Android expects the
files to have different permissions and/or SELinux attributes.

The objects in the **`devices`** array has been extended to also contain the
following:

* **`dynamicMinor`** *(boolean, OPTIONAL)* - copies the [minor
  number](https://www.kernel.org/doc/Documentation/admin-guide/devices.txt) from
  the device node that is present in `path` outside the container. If
  `dynamicMinor` is set to `true`, the value of `minor` is ignored.

#### Example (Chrome OS)

    {
        "linux": {
            "devices": [
                {
                    "path": "/dev/binder",
                    "type": "c",
                    "major": 10,
                    "dynamicMinor": true,
                    "fileMode": 438,
                    "uid": 0,
                    "gid": 0
                }
            ]
        }
    }
