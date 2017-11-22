#!/bin/sh
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run smbprovider with minijail0.

# -i makes sure minijail0 exits right away.
# -p Enter a new PID namespace and run the process as init (pid=1).
# -I Runs program as init inside a new pid namespace.
# -l Enter a new IPC namespace.
# -v Enters new mount namespace, allows to change mounts inside jail.
# -r Remount /proc read-only.
# --uts Enters a new UTS namespace.
# -t Mounts tmpfs as /tmp.
# --mount-dev Creates a new /dev with a minimal set of nodes.
# -b Binds <src> to <dest> in chroot.
# -u Run as smbproviderd user.
# -g Run as smbproviderd group.

exec minijail0 \
    -i \
    -p -I \
    -l \
    -v -r \
    --uts \
    -t \
    --mount-dev -b /dev/log,/dev/log \
    -u smbproviderd -g smbproviderd \
    /usr/sbin/smbproviderd
