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
        'proto_out_dir': 'include/imageloader',
      },
      'sources': ['<(proto_in_dir)/ipc.proto'],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'imageloader-adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/imageloader/dbus_adaptors',
        'dbus_service_config': 'dbus_adaptors/dbus-service-config.json',
      },
      'sources': [
        'dbus_adaptors/org.chromium.ImageLoaderInterface.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
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
        'dlc.cc',
        'dlc.h',
        'helper_process_proxy.cc',
        'helper_process_proxy.h',
        'helper_process_receiver.cc',
        'helper_process_receiver.h',
        'imageloader.cc',
        'imageloader.h',
        'imageloader_impl.cc',
        'verity_mounter.cc',
        'verity_mounter.h',
        'verity_mounter_impl.cc',
        'verity_mounter_impl.h',
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
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libimageloader_static',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'component.h',
            'component_test.cc',
            'dlc_test.cc',
            'imageloader.cc',
            'imageloader.h',
            'imageloader_test.cc',
            'mock_helper_process_proxy.h',
            'test_utilities.cc',
            'test_utilities.h',
            'verity_mounter_test.cc',
          ],
        },
      ],
    }],
    # Fuzzer target.
    ['USE_fuzzer == 1', {
      'targets': [
        {
          'target_name': 'imageloader_helper_process_receiver_fuzzer',
          'type': 'executable',
          'includes': [
            '../common-mk/common_fuzzer.gypi',
          ],
          'dependencies': [
            'libimageloader_static',
          ],
          'sources': [
            'helper_process_receiver_fuzzer.cc',
          ],
        },
      ],
    }],
  ],
}
