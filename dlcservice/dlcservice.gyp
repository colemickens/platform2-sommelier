# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libbrillo-<(libbase_ver)',
        'libimageloader-client',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'dlcservice_adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/dlcservice/dbus_adaptors',
        'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
      },
      'sources': [
        'dbus_adaptors/org.chromium.DlcServiceInterface.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'libdlcservice',
      'type': 'static_library',
      'dependencies': [
        'dlcservice_adaptors',
      ],
      'sources': [
        'dlc_service.cc',
        'dlc_service.h',
        'dlc_service_dbus_adaptor.cc',
        'dlc_service_dbus_adaptor.h',
      ],
    },
    {
      'target_name': 'dlcservice',
      'type': 'executable',
      'dependencies': [
        'libdlcservice',
      ],
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'dlcservice_unittests',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libdlcservice',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
              'libimageloader-client-test',
            ],
          },
          'sources': [
            'dlc_service_dbus_adaptor_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
