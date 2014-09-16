# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
  },
  'targets': [
    {
      'target_name': 'trunks_testrunner',
      'type': 'executable',
      'libraries': [
        '-lchrome_crypto',
      ],
      'includes': ['../common-mk/common_test.gypi'],
      'sources': [
        'hmac_auth_delegate.cc',
        'hmac_auth_delegate_unittest.cc',
        'password_auth_delegate.cc',
        'password_auth_delegate_unittest.cc',
        'trunks_testrunner.cc',
        'tpm_generated.cc',
        'tpm_generated_test.cc',
        'mock_authorization_delegate.cc',
        'mock_command_transceiver.cc'
      ],
    },
    {
      'target_name': 'trunksd',
      'type': 'executable',
      'libraries': [
        '-lminijail',
      ],
      'sources': [
        'trunks_service.cc',
        'tpm_handle_impl.cc',
        'trunksd.cc'
      ],
      'dependencies': [
        'tpm_communication_proto',
      ],
    },
    {
      'target_name': 'trunks_client',
      'type': 'executable',
      'sources': [
        'trunks_client.cc',
      ],
      'dependencies': [
        'trunks',
      ],
    },
    {
      'target_name': 'trunks',
      'type': 'shared_library',
      'libraries': [
        '-lchrome_crypto',
      ],
      'sources': [
        'tpm_generated.cc',
        'null_auth_delegate.cc',
        'password_auth_delegate.cc',
        'hmac_auth_delegate.cc',
        'trunks_proxy.cc',
      ],
      'dependencies': [
        'tpm_communication_proto',
      ],
    },
    {
      'target_name': 'tpm_communication_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/trunks',
      },
      'sources': [
        '<(proto_in_dir)/tpm_communication.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    }
  ],
}
