#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

{
  'target_defaults': {
    'includes': ['../common-mk/common.gypi'],
    'variables': {
      'deps': [  # This is a list of pkg-config dependencies
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'openssl',
        'protobuf-lite',
      ],
    },
    'defines': [ 'USE_TPM2=<(USE_tpm2)' ],
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
        '<(proto_in_dir)/local_data.proto',
        '<(proto_in_dir)/tpm_manager_status.proto',
        '<(proto_in_dir)/tpm_nvram_interface.proto',
        '<(proto_in_dir)/tpm_ownership_interface.proto',
        'common/print_local_data_proto.cc',
        'common/print_tpm_manager_status_proto.cc',
        'common/print_tpm_nvram_interface_proto.cc',
        'common/print_tpm_ownership_interface_proto.cc',
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
        'server/dbus_service.cc',
        'server/local_data_store_impl.cc',
        'server/openssl_crypto_util_impl.cc',
        'server/tpm_manager_service.cc',
      ],
      'conditions': [
        ['USE_tpm2 == 1', {
          'sources': [
            'server/tpm2_initializer_impl.cc',
            'server/tpm2_status_impl.cc',
          ],
          'all_dependent_settings': {
            'libraries': [
              '-ltrunks',
            ],
          },
        }],
        ['USE_tpm2 == 0', {
          'sources': [
            'server/tpm_connection.cc',
            'server/tpm_initializer_impl.cc',
            'server/tpm_nvram_impl.cc',
            'server/tpm_status_impl.cc',
          ],
          'all_dependent_settings': {
            'libraries': [
              '-ltspi',
            ],
          },
        }],
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
      'variables': {
        'deps': [
          'libminijail',
        ],
      },
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
            'server/mock_openssl_crypto_util.cc',
            'server/mock_tpm_initializer.cc',
            'server/mock_tpm_nvram.cc',
            'server/mock_tpm_status.cc',
            'server/tpm_manager_service_test.cc',
            'tpm_manager_testrunner.cc',
          ],
          'conditions': [
            ['USE_tpm2 == 1', {
              'sources': [
                'server/tpm2_status_test.cc',
                'server/tpm2_initializer_test.cc',
              ],
              'libraries': [
                '-ltrunks_test',
              ],
            }],
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
