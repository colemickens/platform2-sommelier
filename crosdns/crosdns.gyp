# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'crosdns_adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/crosdns/dbus_adaptors',
        'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
        'new_fd_bindings': 1,
      },
      'sources': [
        'dbus_adaptors/org.chromium.CrosDns.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'crosdns_proxies',
      'type': 'none',
      'actions': [
        {
          'action_name': 'crosdns_dbus_client',
          'variables': {
            'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
            'proxy_output_file': 'include/crosdns/dbus_adaptors/dbus-proxies.h',
            'new_fd_bindings': 1,
          },
          'sources': [
            'dbus_adaptors/org.chromium.CrosDns.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'libcrosdns_static',
      'type': 'static_library',
      'dependencies': [
        'crosdns_adaptors',
      ],
      'sources': [
        'crosdns_daemon.cc',
        'crosdns_daemon.h',
        'hosts_modifier.cc',
        'hosts_modifier.h',
      ],
    },
    {
      'target_name': 'crosdns',
      'type': 'executable',
      'dependencies': [
        'libcrosdns_static',
        'crosdns_adaptors',
      ],
      'sources': [
        'crosdns_daemon.h',
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'run_tests',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libcrosdns_static',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'hosts_modifier_unittest.cc',
          ],
        },
      ],
    }],
    # Fuzzer target.
    ['USE_fuzzer == 1', {
      'targets': [
        {
          'target_name': 'hosts_modifier_fuzzer',
          'type': 'executable',
          'includes': [
            '../common-mk/common_fuzzer.gypi',
          ],
          'dependencies': [
            'libcrosdns_static',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'hosts_modifier_fuzzer.cc',
          ],
        },
      ],
    }],
  ],
}
