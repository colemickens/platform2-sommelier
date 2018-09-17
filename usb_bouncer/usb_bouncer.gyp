# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libcrypto',
        'libminijail',
        'libsession_manager-client',
        'libusbguard',
        'protobuf',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'usb_bouncer_protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/usb_bouncer',
      },
      'sources': ['<(proto_in_dir)/usb_bouncer.proto'],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'usb_bouncer_common',
      'type': 'static_library',
      'dependencies': [
        'usb_bouncer_protos',
      ],
      'sources': [
        'entry_manager.cc',
        'util.cc',
        'util.h',
      ],
      'headers': [
        'entry_manager.h',
      ],
    },
    {
      'target_name': 'usb_bouncer',
      'type': 'executable',
      'variables': {
        'deps': ['libbrillo-<(libbase_ver)'],
      },
      'dependencies': [
        'usb_bouncer_common',
      ],
      'sources': [
        'usb_bouncer.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'run_tests',
          'type': 'executable',
          'variables': {
            'deps': ['libbrillo-<(libbase_ver)', 'libcrypto'],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'usb_bouncer_common',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'entry_manager_test.cc',
            'entry_manager_test_util.cc',
          ],
        },
      ],
    }],
  ],
}
