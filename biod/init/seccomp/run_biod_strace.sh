#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -x

# Use this script to generate an initial list of syscalls to whitelist with
# seccomp. Note that it will generate two files, each of which ends with the
# PID of the process that ran; you only need to analyze the file with the
# higher PID since the first is the runuser process.

OUTPUT_DIR="$(date --iso-8601=seconds)"
mkdir "${OUTPUT_DIR}"

stop biod || true
strace -ff -o "${OUTPUT_DIR}/strace.log" runuser -u biod -g biod \
    -- /usr/bin/biod --log_dir=/var/log/biod >/var/log/biod.out
