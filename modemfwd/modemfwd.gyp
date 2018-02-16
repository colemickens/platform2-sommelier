# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libbrillo-<(libbase_ver)',
        'libshill-client',
        # system_api depends on protobuf. It must appear before protobuf
        # here or the linker flags won't be in the right order.
        'system_api',
        'protobuf',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'modemfw-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/modemfwd/proto_bindings',
      },
      'sources': [
        '<(proto_in_dir)/firmware_manifest.proto',
        '<(proto_in_dir)/helper_manifest.proto',
        '<(proto_in_dir)/journal_entry.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libmodemfw',
      'type': 'static_library',
      'dependencies': [
        'modemfw-protos',
      ],
      'sources': [
        'firmware_directory.cc',
        'journal.cc',
        'modem.cc',
        'modem_flasher.cc',
        'modem_helper.cc',
        'modem_helper_directory.cc',
        'modem_tracker.cc',
        'proto_file_io.cc',
      ],
    },
    {
      'target_name': 'modemfwd',
      'type': 'executable',
      'dependencies': [
        'libmodemfw',
      ],
      'sources': [
        'component.cc',
        'daemon.cc',
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'modemfw_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libmodemfw',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'firmware_directory_stub.cc',
            'firmware_directory_unittest.cc',
            'journal_unittest.cc',
            'modem_flasher_unittest.cc',
          ],
        },
      ]},
    ],
  ],
}
