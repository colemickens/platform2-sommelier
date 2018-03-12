# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# BootLockbox dbus client.
{
  'targets': [
    {
      'target_name': 'bootlockbox-proxies',
      'type': 'none',
      'actions': [{
        'action_name': 'generate-dbus-proxies',
        'variables': {
          'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
          'proxy_output_file': 'include/bootlockbox/dbus-proxies.h',
          'mock_output_file': 'include/bootlockbox/dbus-proxy-mocks.h',
          'proxy_path_in_mocks': 'bootlockbox/dbus-proxies.h',
        },
        'sources': [
          'dbus_adaptors/org.chromium.BootLockboxInterface.xml',
        ],
        'includes': ['../common-mk/generate-dbus-proxies.gypi'],
      }],
    },
  ],
}
