# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'defines': [
      'OS_CHROMEOS',
    ],
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libcros_config',
        'libcrypto',
        'libcryptohome-client',
        'libmetrics-<(libbase_ver)',
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
        'arc_setup_metrics.cc',
        'arc_setup_util.cc',
        'art_container.cc',
        'config.cc',
      ],
      'variables': {
        'USE_houdini%': 0,
        'USE_ndk_translation%': 0,
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
      ],
      'link_settings': {
        'libraries': [
          '-lbootstat',
        ],
      },
    },
    {
      'target_name': 'arc-setup',
      'type': 'executable',
      'sources': ['main.cc'],
      'dependencies': [
        'libarc_setup',
      ],
    },
    {
      'target_name': 'dev-rootfs.squashfs',
      'type': 'none',
      'actions': [
        {
          'action_name': 'mkdir_squashfs_source_dir',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/squashfs_source_dir',
          ],
          'action': [
            'mkdir', '-p', '<(INTERMEDIATE_DIR)/squashfs_source_dir',
          ],
        },
        {
          'action_name': 'generate_squashfs',
          'inputs': [
            '<(INTERMEDIATE_DIR)/squashfs_source_dir',
          ],
          'outputs': [
            'dev-rootfs.squashfs',
          ],
          'action': [
            'mksquashfs',
            '<(INTERMEDIATE_DIR)/squashfs_source_dir',
            '<(PRODUCT_DIR)/dev-rootfs.squashfs',
            '-no-progress',
            '-info',
            '-all-root',
            '-noappend',
            '-comp', 'lzo',
            '-b', '4K',
            # Create rootfs and necessary dev nodes for art container.
            # ashmem minor number is dynamic determined and will be bind
            # mounted.
            '-p', '/dev d 700 0 0',
            '-p', '/dev/ashmem c 666 root root 1 3',
            '-p', '/dev/random c 666 root root 1 8',
            '-p', '/dev/urandom c 666 root root 1 9',
          ],
        },
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
          # TODO(xzhou): Move boot_lockbox_client.cc and
          # priv_code_verifier.cc back to libarc_setup.
          'sources': [
            'arc_read_ahead_unittest.cc',
            'arc_setup_metrics_unittest.cc',
            'arc_setup_unittest.cc',
            'arc_setup_util_unittest.cc',
            'art_container_unittest.cc',
            'boot_lockbox_client.cc',
            'config_unittest.cc',
            'priv_code_verifier.cc',
          ],
        },
      ],
    }],
  ],
}
