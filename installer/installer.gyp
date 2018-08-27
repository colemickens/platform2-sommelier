# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
      '<(sysroot)/usr/include/verity',
    ],
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'vboot_host',
      ],
    },
    'link_settings': {
      'libraries': [
        '-ldm-bht',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcros_installer',
      'type': 'static_library',
      'sources': [
        'cgpt_manager.cc',
        'chromeos_install_config.cc',
        'chromeos_legacy.cc',
        'chromeos_postinst.cc',
        'chromeos_setimage.cc',
        'chromeos_verity.c',
        'inst_util.cc',
      ],
    },
    {
      'target_name': 'cros_installer',
      'type': 'executable',
      'dependencies': ['libcros_installer'],
      'variables': {
        'deps': ['libchrome-<(libbase_ver)'],
      },
      'sources': [
        'cros_installer_main.cc',
      ],
    },
    {
      'target_name': 'evwaitkey',
      'type': 'executable',
      'sources': [
        'util/evwaitkey.c',
      ],
    },
    {
      'target_name': 'cros_oobe_crypto',
      'type': 'executable',
      'variables': {
        'deps': ['libcrypto'],
      },
      'sources': [
        'util/cros_oobe_crypto.c',
      ],
    },
  ],
  'conditions': [
    ['USE_mtd == 1', {
      'targets': [
        {
          'target_name': 'nand_partition',
          'type': 'executable',
          'dependencies': ['libcros_installer'],
          'variables': {
            'deps': ['libchrome-<(libbase_ver)'],
          },
          'sources': [
            'nand_partition.cc',
            'nand_partition_main.cc',
          ],
        },
      ],
    }],
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'cros_installer_test',
          'type': 'executable',
          'dependencies': [
            'libcros_installer',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'chromeos_install_config_test.cc',
            'chromeos_legacy_test.cc',
            'inst_util_test.cc',
          ],
        },
      ],
    }],
  ],
}
