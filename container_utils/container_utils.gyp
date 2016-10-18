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
      'target_name': 'run_oci',
      'type': 'executable',
      'variables': {
        'deps': [
          'libcontainer',
          'libbrillo-<(libbase_ver)',
          'libcrypto',
        ],
      },
      'sources': [
        'container_config_parser.cc',
        'run_oci.cc',
      ],
    },
    {
      'target_name': 'libdevice_jail',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libudev',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'direct_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'device_jail/device_jail_control.h',
        'device_jail/device_jail_control.cc',
        'device_jail/device_jail_server.h',
        'device_jail/device_jail_server.cc',
      ],
    },
    {
      'target_name': 'device_jail_fs',
      'type': 'executable',
      'variables': {
        'deps': [
          'fuse',
          'libbrillo-<(libbase_ver)',
          'libminijail',
        ],
      },
      'dependencies': [
        'libdevice_jail',
      ],
      'sources': [
        'device_jail/device_jail_fs.cc',
        'device_jail/fs_data.cc',
        'device_jail/fs_data.h',
      ],
    },
    {
      'target_name': 'device_jail_utility',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'dependencies': [
        'libdevice_jail',
      ],
      'sources': [
        'device_jail/device_jail_utility.cc',
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
      ]},
    ],
  ],
}
