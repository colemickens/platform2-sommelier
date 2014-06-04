# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'includes': ['../common-mk/common.gypi'],
    'dependencies': [
      '<(platform2_root)/libchromeos/libchromeos-<(libbase_ver).gyp:*',
    ],
    'variables': {
      'deps': [  # This is a list of pkg-config dependencies
        'libchrome-<(libbase_ver)',
        'openssl'
      ]
    },
    'cflags_cc': ['-std=gnu++11'],
  },
  'targets': [
    {
      'target_name': 'trunks_testrunner',
      'type': 'executable',
      'includes': ['../common-mk/common_test.gypi'],
      'sources': [
        'trunks_testrunner.cc',
        'tpm_generated.cc',
      ],
    },
  ],
}
