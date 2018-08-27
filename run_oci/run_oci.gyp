# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libcap',
        'libchrome-<(libbase_ver)',
        'libminijail',
      ],
      'enable_exceptions': 1,
    },
    'libraries': [
      '-lmount',
    ],
  },
  'targets': [
    {
      'target_name': 'container_config_parser',
      'type': 'shared_library',
      'sources': [
        'container_config_parser.cc',
      ],
    },
    {
      'target_name': 'run_oci',
      'type': 'executable',
      'variables': {
        'deps': [
          'libcontainer',
        ],
      },
      'sources': [
        'container_config_parser.cc',
        'run_oci.cc',
        'run_oci_utils.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'container_config_parser_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'container_config_parser.cc',
            'container_config_parser_test.cc',
          ],
        },
        {
          'target_name': 'run_oci_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'run_oci_test.cc',
            'run_oci_utils.cc',
          ],
        },
      ]},
    ],
  ],
}
