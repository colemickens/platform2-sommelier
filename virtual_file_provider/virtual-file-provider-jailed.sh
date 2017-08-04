#!/bin/sh
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run virtual_file_provider with minijail0.

set -e

MOUNTPOINTS_DIR="/opt/google/containers/virtual-file-provider/mountpoints"
CONTAINER_ROOT="${MOUNTPOINTS_DIR}/container-root"
ROOTFSIMAGE="/usr/share/virtual-file-provider/rootfs.squashfs"

# Mount root filesystem image.
umount -l ${CONTAINER_ROOT} || true
mount -t squashfs "${ROOTFSIMAGE}" "${CONTAINER_ROOT}"

# Start constructing minijail0 args...
args=""

# Enter a new VFS namespace.
args="${args} -v"

# Enter a new network namespace.
args="${args} -e"

# Enter a new PID namespace and run the process as init (pid=1).
args="${args} -p -I"

# Enter a new IPC namespace.
args="${args} -l"

# Forbid all caps except CAP_SYS_ADMIN and CAP_SETPCAP.
args="${args} -c 0x200100"

# Run as virtual-file-provider user/group.
args="${args} -u virtual-file-provider -g virtual-file-provider -G"

# pivot_root to the container root.
args="${args} -P ${CONTAINER_ROOT}"

# Do read-only bind mounts.
# This code assumes no new mount point appears under those directories
# during setup of minijail. Otherwise they can be bind-mounted read-write to
# the container.
# We need this assumption because MS_REMOUNT and MS_REC can not be used
# together.
for i in bin etc lib sbin usr; do
  args="${args} -k /${i},/${i},none,0x1000"   # bind
  args="${args} -k /${i},/${i},none,0x1027"   # bind,remount,nodev,nosuid,ro
done

# For D-Bus system bus socket.
args="$args -k /run/dbus,/run/dbus,none,0x1000"  # bind
# bind,remount,noexec,nodev,nosuid,ro
args="$args -k none,/run/dbus,none,0x102f"

# Mount /proc.
args="${args} -k proc,/proc,proc,0xe"  # noexec,nodev,nosuid

# Mark PRIVATE recursively under (pivot) root, in order not to expose shared
# mount points accidentally.
args="${args} -k none,/,none,0x44000"  # private,rec

# Finally, specify command line arguments.
args="${args} -- /usr/bin/virtual-file-provider /mnt"

exec minijail0 ${args}
