# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Caution!: GYP to GN migration is happening. If you update this file, please
# update common-mk/common_fuzzer/BUILD.gn too accordingly.

# This file is included for every fuzz target.
# You can add anything in here that's valid in a target dictionary.
# Fuzzing only works for amd64 boards at this point.

{
  'conditions': [
    ['USE_asan == 1', {
      'cflags': [
        '-fsanitize=address',
      ],
      'ldflags': [
        '-fsanitize=address',
      ],
    }],
    ['USE_coverage == 1', {
      'cflags': [
        '-fprofile-instr-generate',
        '-fcoverage-mapping',
      ],
      'ldflags': [
        '-fprofile-instr-generate',
        '-fcoverage-mapping',
      ],
    }],
    ['USE_fuzzer == 1', {
      'cflags': [
        '-fsanitize=fuzzer-no-link',
      ],
      'ldflags': [
        '-fsanitize=fuzzer',
      ],
    }],
    ['USE_msan == 1', {
      'cflags': [
        '-fsanitize=memory',
      ],
      'ldflags': [
        '-fsanitize=memory',
      ],
    }],
    ['USE_ubsan == 1', {
      'cflags': [
        '-fsanitize=undefined',
        '-fno-sanitize=vptr',
      ],
      'ldflags': [
        '-fsanitize=undefined',
        '-fno-sanitize=vptr',
      ],
    }],
  ],
}
