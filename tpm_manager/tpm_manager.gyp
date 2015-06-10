# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'includes': ['../common-mk/common.gypi'],
    'variables': {
      'deps': [  # This is a list of pkg-config dependencies
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    # A library for just the protobufs.
    {
      'target_name': 'proto_library',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'common',
        'proto_out_dir': 'include/tpm_manager/common',
      },
      'sources': [
        '<(proto_in_dir)/dbus_interface.proto',
        '<(proto_in_dir)/local_data.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    # A shared library for clients.
    {
      'target_name': 'libtpm_manager',
      'type': 'shared_library',
      'sources': [
        'client/dbus_proxy.cc',
      ],
      'dependencies': [
        'proto_library',
      ],
    },
    # A client command line utility.
    {
      'target_name': 'tpm_manager_client',
      'type': 'executable',
      'sources': [
        'client/main.cc',
      ],
      'dependencies': [
        'libtpm_manager',
        'proto_library',
      ]
    },
    # A library for server code.
    {
      'target_name': 'server_library',
      'type': 'static_library',
      'sources': [
        'server/tpm_manager_service.cc',
        'server/dbus_service.cc',
      ],
      'dependencies': [
        'proto_library',
      ],
    },
    # The tpm_manager daemon.
    {
      'target_name': 'tpm_managerd',
      'type': 'executable',
      'sources': [
        'server/main.cc',
      ],
      'libraries': [
        '-lminijail',
      ],
      'dependencies': [
        'proto_library',
        'server_library',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'tpm_manager_testrunner',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
              'libchromeos-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'client/dbus_proxy_test.cc',
            'common/mock_tpm_manager_interface.cc',
            'server/dbus_service_test.cc',
            'server/mock_local_data_store.cc',
            'server/mock_tpm_initializer.cc',
            'server/mock_tpm_status.cc',
            'server/tpm_manager_service_test.cc',
            'tpm_manager_testrunner.cc',
          ],
          'dependencies': [
            'libtpm_manager',
            'proto_library',
            'server_library',
          ],
        },
      ],
    }],
  ],
}
