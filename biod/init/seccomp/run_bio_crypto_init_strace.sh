#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -x

# Use this script to generate an initial list of syscalls to whitelist with
# seccomp. Note that it will generate three files, each of which ends with the
# PID of the process that ran. The first is for the runuser process, which
# doesn't need to be analyzed. The two files with the higher PIDs correspond to
# bio_crypto_init (two since it forks).

OUTPUT_DIR="$(date --iso-8601=seconds)"
mkdir "${OUTPUT_DIR}"

# Use a random seed (instead of real TPM seed)
SEED="/run/bio_crypto_init/seed"
dd if=/dev/urandom of="${SEED}" bs=32 count=1
chown biod:biod "${SEED}"

strace -ff -o "${OUTPUT_DIR}/strace.log" runuser -u biod -g biod \
    -- /usr/bin/bio_crypto_init --log_dir=/var/log/bio_crypto_init
