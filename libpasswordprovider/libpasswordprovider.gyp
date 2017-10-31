# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [{
    'target_name': 'libpasswordprovider',
    'type': 'static_library',
    'dependencies': [
    ],
    'link_settings': {
      'libraries': [
        '-lkeyutils',
      ],
    },
    'sources': [
      'password.cc',
      'password.h',
      'password_provider.cc',
      'password_provider.h',
    ],
  }],
  #Unit tests.
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'password_provider_test',
          'type': 'executable',
          'dependencies': [
            'libpasswordprovider',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'password_provider_test.cc',
            'password_test.cc',
          ],
        },
      ],
    }],
  ],
}
