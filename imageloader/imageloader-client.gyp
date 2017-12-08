# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'imageloader-proxies',
      'type': 'none',
      'actions': [
        {
          'action_name': 'imageloader-dbus-client',
          'variables': {
            'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
            'proxy_output_file': 'include/imageloader/dbus-proxies.h',
            'mock_output_file': 'include/imageloader/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'imageloader/dbus-proxies.h',
          },
          'sources': [
            'dbus_adaptors/org.chromium.ImageLoaderInterface.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
  ],
}
