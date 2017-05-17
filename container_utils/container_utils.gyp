# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libsession_manager-client',
      ],
      'enable_exceptions': 1,
    },
  },
  'targets': [
    {
      'target_name': 'mount_extension_image',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'mount_extension_image.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_device_jail == 1', {
      'targets': [
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
            'device_jail/device_jail_control.cc',
            'device_jail/device_jail_control.h',
            'device_jail/device_jail_server.cc',
            'device_jail/device_jail_server.h',
          ],
        },
      ],
    }],
  ],
}
