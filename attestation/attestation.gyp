# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
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
      # Use -fPIC so this code can be linked into a shared library.
      'cflags!': ['-fPIE'],
      'cflags': [
        '-fPIC',
        '-fvisibility=default',
      ],
      'variables': {
        'proto_in_dir': 'common',
        'proto_out_dir': 'include/attestation/common',
      },
      'sources': [
        '<(proto_in_dir)/attestation_ca.proto',
        '<(proto_in_dir)/common.proto',
        '<(proto_in_dir)/database.proto',
        '<(proto_in_dir)/interface.proto',
        'common/print_common_proto.cc',
        'common/print_interface_proto.cc',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    # A library for common code.
    {
      'target_name': 'common_library',
      'type': 'static_library',
      'sources': [
        'common/crypto_utility_impl.cc',
        'common/tpm_utility_v1.cc',
      ],
      'all_dependent_settings': {
        'variables': {
          'deps': [
            'openssl',
          ],
        },
        'libraries': [
          '-ltspi',
        ],
      },
      'dependencies': [
        'proto_library',
      ],
    },
    # A library for client code.
    {
      'target_name': 'client_library',
      'type': 'static_library',
      # Use -fPIC so this code can be linked into a shared library.
      'cflags!': ['-fPIE'],
      'cflags': [
        '-fPIC',
        '-fvisibility=default',
      ],
      'sources': [
        'client/dbus_proxy.cc',
      ],
      'dependencies': [
        'proto_library',
      ],
    },
    # A shared library for clients.
    {
      'target_name': 'libattestation',
      'type': 'shared_library',
      'cflags': ['-fvisibility=default'],
      'sources': [
      ],
      'dependencies': [
        'client_library',
        'proto_library',
      ],
    },
    # A client command line utility.
    {
      'target_name': 'attestation_client',
      'type': 'executable',
      'sources': [
        'client/main.cc',
      ],
      'dependencies': [
        'client_library',
        'common_library',
        'proto_library',
      ]
    },
    # A library for server code.
    {
      'target_name': 'server_library',
      'type': 'static_library',
      'sources': [
        'server/attestation_service.cc',
        'server/dbus_service.cc',
        'server/database_impl.cc',
        'server/pkcs11_key_store.cc',
      ],
      'all_dependent_settings': {
        'libraries': [
          '-lchaps',
        ],
      },
      'dependencies': [
        'proto_library',
      ],
    },
    # The attestation daemon.
    {
      'target_name': 'attestationd',
      'type': 'executable',
      'sources': [
        'server/main.cc',
      ],
      'variables': {
        'deps': [
          'libminijail',
        ],
      },
      'dependencies': [
        'common_library',
        'proto_library',
        'server_library',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'attestation_testrunner',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
              'libchromeos-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'attestation_testrunner.cc',
            'client/dbus_proxy_test.cc',
            'common/crypto_utility_impl_test.cc',
            'common/mock_crypto_utility.cc',
            'common/mock_tpm_utility.cc',
            'server/attestation_service_test.cc',
            'server/database_impl_test.cc',
            'server/dbus_service_test.cc',
            'server/mock_database.cc',
            'server/mock_key_store.cc',
            'server/pkcs11_key_store_test.cc',
          ],
          'dependencies': [
            'common_library',
            'client_library',
            'proto_library',
            'server_library',
          ],
        },
      ],
    }],
  ],
}
