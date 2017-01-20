# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
      'enable_exceptions': 1,
    },
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
      'target_name': 'libpermission_broker_client',
      'type': 'static_library',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'device_jail/permission_broker_client.cc',
        'device_jail/permission_broker_client.h',
      ],
    },
    {
      'target_name': 'run_oci',
      'type': 'executable',
      'variables': {
        'deps': [
          'libcontainer',
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'container_config_parser.cc',
        'run_oci.cc',
      ],
    },
    {
      'target_name': 'device_jail',
      'type': 'executable',
      'variables': {
        'deps': [
          'fuse',
          'libbrillo-<(libbase_ver)',
        ],
      },
      'dependencies': [
        'libpermission_broker_client',
      ],
      'sources': [
        'device_jail/device_jail.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'container_config_parser_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'variables': {
            'deps': [
              'libbrillo-test-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'container_config_parser.cc',
            'container_config_parser_unittest.cc',
          ],
        },
        {
          'target_name': 'permission_broker_client_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libpermission_broker_client',
          ],
          'variables': {
            'deps': [
              'libbrillo-test-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'device_jail/permission_broker_client_unittest.cc',
          ],
        },
      ]},
    ],
  ],
}
