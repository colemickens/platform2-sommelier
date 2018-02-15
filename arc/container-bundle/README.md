# Android container in Chrome OS

This document outlines the process by which Android runs in a Linux container in
Chrome OS.

This document explains how the container for Android master works unless
otherwise noted. The container for N may work in a slightly different way.

## config.json

`config.json` is used by
[`run_oci`](https://chromium.googlesource.com/chromiumos/platform2/+/master/run_oci/),
to describe how the container is set up. This file describes the mount
structure, namespaces, device nodes that are to be created, cgroups
configuration, and capabilities that are inherited.

## Mounts

There are several ways in which resources are mounted inside the container:

* Loop mounts: used to mount filesystem images into the filesystem. Android uses
  two of these: one for `system.raw.img`, and another one for `vendor.raw.img`.
* Bind mounts: these make a file or directory visible from another subdirectory
  and can span [`chroot(2)`](http://man7.org/linux/man-pages/man2/chroot.2.html)
  and [`pivot_root(2)`](http://man7.org/linux/man-pages/man2/pivot_root.2.html).
* Shared mounts: these mounts use the `MS_SHARED` flags for
  [`mount(2)`](http://man7.org/linux/man-pages/man2/mount.2.html) in the parent
  mount namespace and `MS_SLAVE` in the child mount namespace, which causes any
  mount changes under that mount point to propagate to other shared subtrees.

Mounts can also happen in several
[`mount_namespaces(7)`](http://man7.org/linux/man-pages/man7/mount_namespaces.7.html):

* The init mount namespace: The `system.raw.img` is
  mounted in the init namespace. The mount that is performed here can span
  user sessions (e.g. for the system image). All of the mounts that are
  performed in the init mount namespace are performed before the container
  starts, and do not figure in (but can be consumed by) `config.json`.
* The intermediate mount namespace: This mount namespace is associated with the
  init
  [`user_namespaces(7)`](http://man7.org/linux/man-pages/man7/user_namespaces.7.html),
  so it has all of `root`'s capabilities. This child of the init mount namespace
  is only accessible while `run_oci` is running since after it exits there are
  no live processes that have a direct reference to this namespace. This is also
  the only mount namespace in which remounts (passing the `MS_REMOUNT` without
  passing the `MS_BIND` flag to `mount(2)`) can occur in a way that they don't
  leak into the init namespace, since `/` is recursively remounted as
  `MS_SLAVE`. These mounts are performed by passing the
  `performInIntermediateNamespace` flag in `config.json`.
* The container mount namespace: This namespace is where the vast majority of
  the mounts happen. This child of the intermediate mount namespace is
  associated with the container user namespace, so some operations are no longer
  allowed to be performed here, such as remounts that modify the `exec`, `suid`,
  `dev` flags. These mounts are performed by adding a regular mount to
  `config.json`, or as part of the hooks.

All mounts are performed in the `/opt/google/container/android/rootfs/root`
subtree.

### List of mounts visible in the container mount namespace

* `/`: This is mounted by `/etc/init/arc-system-mount.conf` in
  the init namespace, and span container invocations since it is stateless.
  The `exec`/`suid` flags are added in the intermediate mount namespace.
* `/dev`: This is a `tmpfs` mounted in the intermediate mount namespace with
  `android-root` as owner. This is needed to get the `dev`/`exec` mount flags.
* `/dev/pts`: Pseudo TTS devpts file system with namespace support so that it is
  in a different namespace than the parent namespace even though the device node
  ids look identical. Required for bionic CTS tests. The device is mounted with
  nosuid and noexec mount options for better security although stock Android
  does not use them.
* `/dev/ptmx`: The [kernel
  documentation](https://www.kernel.org/doc/Documentation/filesystems/devpts.txt)
  for devpts indicates that there are two ways to support `/dev/ptmx`: creating
  a symlink that points to `/dev/pts/ptmx`, or bind-mounting `/dev/pts/ptmx`.
  The bind-mount was chosen because it is easier to find in `config.json`. The
  device is mounted with nosuid and noexec mount options for better security
  although stock Android does not use them.
* `/dev/kmsg`: This is a bind-mount of the host's `/run/arc/android.kmsg.fifo`,
  which is just a FIFO file. Logs written to the fake device are read by a job
  called `arc-kmsg-logger` and stored in host's /var/log/android.kmsg.
* `/dev/{socket,input}`: These are normal `tmpfs`.
* `/data` and `/data/cache`: These two directories are bind-mounted from the
  Chrome OS user's cryptohome (from
  `/home/root/${HASH}/android-data/{data,cache}`), which is only available after
  a user has logged in.
* `/var/run/arc`: A `tmpfs` that holds several mount points from other
  containers for Chrome <=> Android file system communication, such as `dlfs`, OBB,
  and external storage.
* `/var/run/arc/sdcard`: A FUSE file system provided by `sdcard` daemon running
  outside the container.
* `/var/run/chrome`: Holds the ARC bridge and Wayland UNIX domain sockets.
* `/var/run/cras`: Holds the CRAS UNIX domain socket.
* `/var/run/inputbridge`: Holds a FIFO for doing IPC within the container.
   surfaceflinger uses the FIFO to propage input events from host to the
   container.
* `/sys`: A normal `sysfs`.
* `/sys/fs/selinux`: This is bind-mounted from `/sys/fs/selinux` outside the
  container.
* `/sys/kernel/debug`: Since this directory is owned by real root with very
  restrictive permissions (so the container would not be able to access any
  resource in that directory), a `tmpfs` is mounted in its place.
* `/sys/kernel/debug/sync`: The permissions of this directory in the host are
  relaxed so that `android-root` can access it, and bind-mounted in the
  container.
* `/proc`: A normal `proc` fs. This is mounted in the container mount namespace,
  which is associated with the container user+pid namespaces to display the
  correct PID mappings.
* `/proc/cmdline`: A regular file with the runtime-generated kernel commandline
  is bind-mounted instead of the Chrome OS kernel commandline.
* `/proc/sys/vm/mmap_rnd_compat_bits`, `/proc/sys/vm/mmap_rnd_bits`: Two regular
  files are bind-mounted since the original files are owned by real root with
  very restrictive permissions. Android's `init` modified the contents of these
  files to increase the
  [`mmap(2)`](http://man7.org/linux/man-pages/man2/mmap.2.html) entropy, and
  will crash if this operation is not allowed. Mounting these two files reduces
  the number of mods to `init`.
* `/proc/sys/kernel/kptr_restrict`: Same as with `/proc/sys/vm/mmap_rnd_bits`.
* `/oem`: Holds `platform.xml` file.
* `/var/run/arc/bugreport`: This is bind-mounted from host's
  `/run/arc/bugreport`. The container creates a pipe file in the directory to
  allow host's `debugd` to read it. When it is read, Android's `bugreport`
  output is sent to the host side.
* `/var/run/arc/apkcache`: This is bind-mounted from host's
  `/mnt/stateful_partition/unencrypted/cache/apk`. The host directory is for
  storing APK files specified by the device's policy and downloaded on the host
  side.
* `/var/run/arc/dalvik-cache`: This is bind-mounted from host's
  `/mnt/stateful_partition/unencrypted/art-data/dalvik-cache`. The host
  directory is for storing boot*.art files compiled on the host side. This
  allows the container to load the files right away without building them.
* `/var/run/camera`: Holds the arc-camera UNIX domain socket.
* `/var/run/arc/obb`: This is bind-mounted from host's `/run/arc/obb`. A daemon
  running outside the container called `/usr/bin/arc-obb-mounter` mounts an OBB
  image file as a FUSE file system to the directory when requested.
* `/var/run/arc/media`: This is bind-mounted from host's `/run/arc/media`. A
  daemon running outside the container called `/usr/bin/mount-passthrough` mounts
  an external storage as a FUSE file system to the directory when needed.
* `/vendor`: This is loop-mounted from host's
  `opt/google/containers/android/vendor.raw.img`. The directory may have graphic
   drivers, Houdini, board-specific APKs, and so on.

## Capabilities

Android is running in a user namespace, and the `root` user in the namespace has
all possible capabilities in that namespace. Nevertheless, there are some
operations in the kernel where the capability check is performed against the
user in the init namespace. All the capabilities where all the checks are done
in this way (such as `CAP_SYS_MODULE`) are removed because no user within the
container would be able to use it.

Additionally, the following capabilities were removed (by dropping them from the
list of permitted, inheritable, effective, and ambient capability sets) to signal
the container that it cannot perform certain operations:

* `CAP_SYS_BOOT`: This signals Android's `init` process that it should not use
  `reboot(2)`, but instead call `exit(2)`. It is also used to decide whether or
  not to block the `SIGTERM` signal, which can be used to request the container
  to terminate itself from the outside.
* `CAP_SYSLOG`: This signals Android that it will not be able to access kernel
  pointers found in `/proc/kallsyms`.

## Cgroups

By default, processes running inside the container are not allowed to access any
device files. They can only access the ones that are explcitly allowed in the
`config.json`'s `linux` > `resources` > `devices` section.

## Namespaces

TODO

## Boot process

TODO

### Hooks

TODO

## References

* [Shared subtrees kernel documentation](
  https://www.kernel.org/doc/Documentation/filesystems/sharedsubtree.txt)
* [ARC M/N File system design document](http://go/arc++filesystem)
