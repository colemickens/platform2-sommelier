# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is included for every fuzz target.
# You can add anything in here that's valid in a target dictionary.
# Fuzzing only works for amd64 boards at this point.

{
  'cflags': [
    '-fsanitize=fuzzer-no-link,address',
  ],
  'ldflags': [
    '-fsanitize=fuzzer,address',
  ],
}
