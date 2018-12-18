# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Caution!: GYP to GN migration is happening. If you update this file, please
# update the corresponding GN portion in common-mk/BUILD.gn or
# container_utils/BUILD.gn .

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
          'libimageloader-client',
        ],
      },
      'sources': [
        'mount_extension_image.cc',
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
        'device_jail_fs.cc',
        'fs_data.cc',
        'fs_data.h',
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
        'device_jail_utility.cc',
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
        'device_jail_control.cc',
        'device_jail_control.h',
        'device_jail_server.cc',
        'device_jail_server.h',
      ],
    },
  ],
}
