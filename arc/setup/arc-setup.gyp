# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'defines': [
      'OS_CHROMEOS',
    ],
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libcrypto',
        'libcryptohome-client',
        'libminijail',
        'libselinux',
        # system_api depends on protobuf (or protobuf-lite). It must appear
        # before protobuf here or the linker flags won't be in the right
        # order.
        'system_api',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libarc_setup',
      'type': 'static_library',
      'sources': [
        'arc_read_ahead.cc',
        'arc_setup.cc',
        'arc_setup_util.cc',
        'art_container.cc',
      ],
      'variables': {
        'USE_houdini%': 0,
        'USE_ndk_translation%': 0,
        'USE_android_container_master_arc_dev%': 0,
        'USE_cheets_aosp_userdebug%': 0,
        'USE_cheets_aosp_userdebug_64%': 0,
      },
      'conditions': [
        ['USE_houdini == 1', {
          'defines': [
            'USE_HOUDINI=1',
          ],
        }],
        ['USE_ndk_translation == 1', {
          'defines': [
            'USE_NDK_TRANSLATION=1',
          ],
        }],
        ['USE_android_container_master_arc_dev == 1', {
          'defines': [
            'USE_MASTER_CONTAINER=1',
          ],
          'conditions': [
            ['USE_cheets_aosp_userdebug == 1 or USE_cheets_aosp_userdebug_64 == 1', {
              'defines': [
                'USE_MASTER_AOSP_CONTAINER=1',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'arc-setup',
      'type': 'executable',
      'sources': ['main.cc'],
      'dependencies': [
        'libarc_setup',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'arc-setup_testrunner',
          'type': 'executable',
          'dependencies': [
            'libarc_setup',
            '../../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libbrillo-test-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'arc_read_ahead_unittest.cc',
            'arc_setup_unittest.cc',
            'arc_setup_util_unittest.cc',
            'art_container_unittest.cc',
            # TODO(xzhou): Move boot_lockbox_client.cc and
            # priv_code_verifier.cc back to libarc_setup.
            'boot_lockbox_client.cc',
            'priv_code_verifier.cc',
          ],
        },
      ],
    }],
  ],
}
