# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'devmapper',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libcrypto',
        'libminijail',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include',
      },
      'sources': ['<(proto_in_dir)/ipc.proto'],
      'includes': ['../../platform2/common-mk/protoc.gypi'],
    },
    {
      'target_name': 'imageloader-adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/dbus_adaptors',
        'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
      },
      'sources': [
        'dbus_adaptors/org.chromium.ImageLoaderInterface.xml',
      ],
      'includes': ['../../platform2/common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'imageloader-proxies',
      'type': 'none',
      'actions': [
        {
          'action_name': 'imageloader-dbus-client',
          'variables': {
        'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
        'proxy_output_file': 'include/dbus_adaptors/dbus-proxies.h',
          },
          'sources': [
        'dbus_adaptors/org.chromium.ImageLoaderInterface.xml',
          ],
          'includes': ['../../platform2/common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'libimageloader_static',
      'type': 'static_library',
      'dependencies': [
        'imageloader-adaptors',
        'protos',
      ],
      'sources': [
        'component.cc',
        'component.h',
        'helper_process.cc',
        'helper_process.h',
        'imageloader.cc',
        'imageloader.h',
        'imageloader_impl.cc',
        'mount_helper.cc',
        'mount_helper.h',
        'verity_mounter.h',
        'verity_mounter.cc',
        'verity_mounter_impl.h',
        'verity_mounter_impl.cc',
      ],
    },
    {
      'target_name': 'imageloader',
      'type': 'executable',
      'variables': {
        'deps': ['libbrillo-<(libbase_ver)'],
      },
      'dependencies': [
        'libimageloader_static',
        'imageloader-adaptors',
      ],
      'sources': [
        'imageloader.h',
        'imageloader_main.cc',
      ],
    },
    ],
    'conditions': [
      ['USE_test == 1', {
        'targets': [
        {
          'target_name': 'run_tests',
          'type': 'executable',
          'variables': {
            'deps': ['libbrillo-<(libbase_ver)', 'libcrypto'],
          },
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'dependencies': [
            'libimageloader_static',
          ],
          'sources': [
            'run_tests.cc',
            'component_unittest.cc',
            'component.h',
            'imageloader_unittest.cc',
            'imageloader.cc',
            'imageloader.h',
            'mock_helper_process.h',
            'test_utilities.cc',
            'test_utilities.h',
            'verity_mounter_unittest.cc',
          ],
        },
        ],
      }],
    ],
}
