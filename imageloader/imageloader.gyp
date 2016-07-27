# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'dbus-c++-1',
      ],
      # imageloader uses try/catch to interact with dbus-c++
      'enable_exceptions': 1,
    },
  },
    'targets': [
    {
      'target_name': 'libimageloader_common',
      'type': 'static_library',
      'sources': [
        'imageloader_common.cc',
        'imageloader_common.h',
      ],
    },
    {
      'target_name': 'imageloader-glue',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_in_dir': '.',
        'xml2cpp_out_dir': 'include',
      },
      'sources': [
        '<(xml2cpp_in_dir)/imageloader-glue.xml',
      ],
      'includes': ['../../platform2/common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'imageloadclient-glue',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': '.',
        'xml2cpp_out_dir': 'include',
      },
      'sources': [
        '<(xml2cpp_in_dir)/imageloadclient-glue.xml',
      ],
      'includes': ['../../platform2/common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'libimageloader_static',
      'type': 'static_library',
      'dependencies': [
        'libimageloader_common',
      ],
      'sources': [
        'imageloader_impl.cc',
        'imageloader.h',
        'loop_mounter.h',
        'loop_mounter.cc',
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
        'imageloader-glue',
      ],
      'sources': [
        'imageloader.h',
        'imageloader-glue.h',
        'imageloader_main.cc',
      ],
    },
    {
      'target_name': 'imageloadclient',
      'type': 'executable',
      'dependencies': [
        'imageloadclient-glue',
        'libimageloader_common',
      ],
      'sources': [
        'imageloadclient.cc',
        'imageloadclient.h',
        'imageloadclient-glue.h',
      ],
      'libraries': ['-lpthread'],
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
            'libimageloader_common',
          ],
          'sources': [
            'run_tests.cc',
            'imageloader_unittest.cc',
            'imageloader.h',
            'mock_loop_mounter.h',
          ],
        },
        ],
      }],
    ],
}
