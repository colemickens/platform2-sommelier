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
        'openssl'
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
    },
    {
      'target_name': 'trunks_client',
      'type': 'executable',
      'sources': [
        'trunks_client.cc',
        'trunks_proxy.cc'
      ]
    },
  ],
}
