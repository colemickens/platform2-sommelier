#
# Copyright (C) 2014 The Android Open Source Project
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
      # This is a list of pkg-config dependencies
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'openssl',
        'protobuf-lite',
      ],
    },
    'conditions': [
      ['USE_tpm2_simulator == 1', {
        'defines': [
          'USE_SIMULATOR=1',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'interface_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/trunks',
      },
      'sources': [
        '<(proto_in_dir)/interface.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'pinweaver_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/trunks',
      },
      'sources': [
        '<(proto_in_dir)/pinweaver.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'trunks',
      'type': 'shared_library',
      'sources': [
        'background_command_transceiver.cc',
        'blob_parser.cc',
        'error_codes.cc',
        'hmac_authorization_delegate.cc',
        'hmac_session_impl.cc',
        'password_authorization_delegate.cc',
        'policy_session_impl.cc',
        'scoped_key_handle.cc',
        'session_manager_impl.cc',
        'tpm_generated.cc',
        'tpm_pinweaver.cc',
        'tpm_state_impl.cc',
        'tpm_utility_impl.cc',
        'trunks_dbus_proxy.cc',
        'trunks_factory_impl.cc',
      ],
      'dependencies': [
        'interface_proto',
        'pinweaver_proto',
      ],
      'conditions': [
        ['USE_ftdi_tpm == 1', {
          'sources': [
            'ftdi/mpsse.c',
            'ftdi/support.c',
            'trunks_ftdi_spi.cc',
          ],
          'libraries': [
            '-lftdi1',
          ],
          'defines': [
            'SPI_OVER_FTDI=1',
          ],
        }],
      ],
    },
    {
      'target_name': 'pinweaver_client',
      'type': 'executable',
      'sources': [
        'pinweaver_client.cc',
      ],
      'dependencies': [
        'trunks',
        'pinweaver_proto',
      ],
    },
    {
      'target_name': 'trunks_client',
      'type': 'executable',
      'sources': [
        'trunks_client.cc',
        'trunks_client_test.cc',
      ],
      'dependencies': [
        'trunks',
      ],
    },
    {
      'target_name': 'trunks_send',
      'type': 'executable',
      'sources': [
        'trunks_send.cc',
      ],
      'dependencies': [
        'trunks',
      ],
    },
    {
      'target_name': 'trunksd_lib',
      'type': 'static_library',
      'variables': {
        'deps': [
          'libpower_manager-client',
        ],
      },
      'sources': [
        'power_manager.cc',
        'resource_manager.cc',
        'tpm_handle.cc',
        'tpm_simulator_handle.cc',
        'trunks_dbus_service.cc',
      ],
      'dependencies': [
        'interface_proto',
      ],
      'all_dependent_settings': {
        'libraries': [
          '-lsystem_api-power_manager-protos',
        ],
        'variables': {
          'deps': [
            'libpower_manager-client',
          ],
        },
      },
    },
    {
      'target_name': 'trunksd',
      'type': 'executable',
      'sources': [
        'trunksd.cc',
      ],
      'variables': {
        'deps': [
          'libminijail',
        ],
      },
      'dependencies': [
        'interface_proto',
        'trunks',
        'trunksd_lib',
      ],
      'conditions': [
        ['USE_ftdi_tpm == 1', {
          'defines': [
            'SPI_OVER_FTDI=1',
          ],
        }],
        ['USE_tpm2_simulator == 1', {
          'libraries': [
            '-ltpm2',
          ],
        }],
      ],
    },
    {
      'target_name': 'trunks_test',
      'type': 'static_library',
      'standalone_static_library': 1,
      'sources': [
        'mock_authorization_delegate.cc',
        'mock_blob_parser.cc',
        'mock_command_transceiver.cc',
        'mock_hmac_session.cc',
        'mock_policy_session.cc',
        'mock_session_manager.cc',
        'mock_tpm.cc',
        'mock_tpm_state.cc',
        'mock_tpm_utility.cc',
        'trunks_factory_for_test.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'trunks_testrunner',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libpower_manager-client-test',
            ],
          },
          'sources': [
            'background_command_transceiver_test.cc',
            'hmac_authorization_delegate_test.cc',
            'hmac_session_test.cc',
            'password_authorization_delegate_test.cc',
            'policy_session_test.cc',
            'power_manager_test.cc',
            'resource_manager_test.cc',
            'scoped_global_session_test.cc',
            'scoped_key_handle_test.cc',
            'session_manager_test.cc',
            'tpm_generated_test.cc',
            'tpm_state_test.cc',
            'tpm_utility_test.cc',
            'trunks_dbus_proxy_test.cc',
            'trunks_factory_test.cc',
            'trunks_testrunner.cc',
          ],
          'dependencies': [
            'trunks',
            'trunks_test',
            'trunksd_lib',
          ],
        },
      ],
    }],
    ['USE_fuzzer == 1', {
      'targets': [
        {
          'target_name': 'trunks_key_blob_fuzzer',
          'type': 'executable',
          'includes': ['../common-mk/common_fuzzer.gypi'],
          'sources': [
            'key_blob_fuzzer.cc',
          ],
          'dependencies': [
            'trunks',
          ],
        },
        {
          'target_name': 'trunks_creation_blob_fuzzer',
          'type': 'executable',
          'includes': ['../common-mk/common_fuzzer.gypi'],
          'sources': [
            'creation_blob_fuzzer.cc',
          ],
          'dependencies': [
            'trunks',
          ],
        },
        {
          'target_name': 'trunks_resource_manager_fuzzer',
          'type': 'executable',
          'includes': [
            '../common-mk/common_fuzzer.gypi',
            '../common-mk/common_test.gypi',
          ],
          'sources': [
            'fuzzed_command_transceiver.cc',
            'resource_manager_fuzzer.cc',
          ],
          'dependencies': [
            'trunks',
            'trunks_test',
            'trunksd_lib',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',  # For FuzzedDataProvider
            ],
          },
        },
      ],
    }],
  ],
}
