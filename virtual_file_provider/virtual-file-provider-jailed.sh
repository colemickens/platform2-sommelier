#!/bin/sh
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run virtual_file_provider with minijail0.

set -e

MOUNT_FLAGS="MS_NOSUID|MS_NODEV|MS_NOEXEC"

# Start constructing minijail0 args...
args=""

# Use minimalistic-mountns profile.
args="$args --profile=minimalistic-mountns"

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

# Mount tmpfs on /mnt.
args="$args -k tmpfs,/mnt,tmpfs,${MOUNT_FLAGS}"

# Mount tmpfs on /run.
args="$args -k tmpfs,/run,tmpfs,${MOUNT_FLAGS}"

# For D-Bus system bus socket.
args="$args -b /run/dbus"

# Bind /dev/fuse to mount FUSE file systems.
args="$args -b /dev/fuse"

# Finally, specify command line arguments.
args="${args} -- /usr/bin/virtual-file-provider /mnt"

exec minijail0 ${args}
