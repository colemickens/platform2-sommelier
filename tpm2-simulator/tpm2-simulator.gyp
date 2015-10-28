# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'includes': ['../common-mk/common.gypi'],
    'variables': {
      'deps': [  # This is a list of pkg-config dependencies
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'openssl',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'tpm2-simulator',
      'type': 'executable',
      'sources': [
        'main.cc',
      ],
      'libraries': [
        '-ltpm2',
      ],
    },
  ],
}
