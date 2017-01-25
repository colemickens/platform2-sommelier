# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'expat',
        'libcrypto',
      ],
    },
    'include_dirs': [
      '<(platform2_root)/../weave/libweave',
      '<(platform2_root)/../weave/libweave/include',
      '<(platform2_root)/../weave/libweave/third_party/modp_b64/modp_b64/',
      '<(platform2_root)/../weave/libweave/third_party/libuweave/',
    ],
  },
  'targets': [
    {
      'target_name': 'libweave_common',
      'type': 'static_library',
      'cflags!': ['-fPIE'],
      'cflags': ['-fPIC'],
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        'src/backoff_entry.cc',
        'src/base_api_handler.cc',
        'src/commands/cloud_command_proxy.cc',
        'src/commands/command_definition.cc',
        'src/commands/command_dictionary.cc',
        'src/commands/command_instance.cc',
        'src/commands/command_manager.cc',
        'src/commands/command_queue.cc',
        'src/commands/object_schema.cc',
        'src/commands/prop_constraints.cc',
        'src/commands/prop_types.cc',
        'src/commands/prop_values.cc',
        'src/commands/schema_constants.cc',
        'src/commands/schema_utils.cc',
        'src/config.cc',
        'src/data_encoding.cc',
        'src/device_manager.cc',
        'src/device_registration_info.cc',
        'src/error.cc',
        'src/http_constants.cc',
        'src/json_error_codes.cc',
        'src/notification/notification_parser.cc',
        'src/notification/pull_channel.cc',
        'src/notification/xml_node.cc',
        'src/notification/xmpp_channel.cc',
        'src/notification/xmpp_iq_stanza_handler.cc',
        'src/notification/xmpp_stream_parser.cc',
        'src/privet/cloud_delegate.cc',
        'src/privet/constants.cc',
        'src/privet/device_delegate.cc',
        'src/privet/device_ui_kind.cc',
        'src/privet/openssl_utils.cc',
        'src/privet/privet_handler.cc',
        'src/privet/privet_manager.cc',
        'src/privet/privet_types.cc',
        'src/privet/publisher.cc',
        'src/privet/security_manager.cc',
        'src/privet/wifi_bootstrap_manager.cc',
        'src/privet/wifi_ssid_generator.cc',
        'src/registration_status.cc',
        'src/states/error_codes.cc',
        'src/states/state_change_queue.cc',
        'src/states/state_manager.cc',
        'src/states/state_package.cc',
        'src/streams.cc',
        'src/string_utils.cc',
        'src/utils.cc',
        'third_party/chromium/crypto/p224.cc',
        'third_party/chromium/crypto/p224_spake.cc',
        'third_party/chromium/crypto/sha2.cc',
        'third_party/modp_b64/modp_b64.cc',
      ],
    },
    {
      'target_name': 'libweave-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
      'includes': [
        '../../platform2/common-mk/deps.gypi',
      ],
      'dependencies': [
        'libweave_common',
      ],
      'sources': [
        'src/empty.cc',
      ],
    },
    {
      'target_name': 'libweave-test-<(libbase_ver)',
      'type': 'static_library',
      'standalone_static_library': 1,
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        'src/test/fake_stream.cc',
        'src/test/fake_task_runner.cc',
        'src/test/mock_command.cc',
        'src/test/unittest_utils.cc',
      ],
      'includes': ['../../platform2/common-mk/deps.gypi'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libweave_testrunner',
          'type': 'executable',
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
            ],
          },
          'dependencies': [
            'libweave_common',
            'libweave-test-<(libbase_ver)',
          ],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            '<!@(find src/ -name *unittest.cc)',
            '<!@(find third_party/chromium/crypto/ -name *unittest.cc)',
            'src/test/weave_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
