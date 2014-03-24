{
  'variables': {
    'libbase_ver': 242728,
  },
  'target_defaults': {
    'dependencies': [
      '../../metrics/libmetrics-<(libbase_ver).gyp:libmetrics-<(libbase_ver)',
    ],
    'variables': {
      'deps': [
        'dbus-1',
        'libchrome-<(libbase_ver)',
        'libcurl',
      ],
    },
    # TODO(sosa): Remove gflags: crbug.com/356745.
    'link_settings': {
      'libraries': [
        '-lgflags',
      ],
    },
    'include_dirs': [
      '..'  # To access all src/platform2 directories
    ],
    # TODO(sosa): Remove no-strict-aliasing: crbug.com/356745.
    'cflags_cc': [
      '-std=gnu++11',
      '-fno-strict-aliasing',
    ],
  },
  'targets': [
    {
      'target_name': 'buffet_common',
      'type': 'static_library',
      'sources': [
        'data_encoding.cc',
        'dbus_manager.cc',
        'http_request.cc',
        'http_transport_curl.cc',
        'http_utils.cc',
        'mime_utils.cc',
        'string_utils.cc',
      ],
    },
    {
      'target_name': 'buffet',
      'type': 'executable',
      'sources': [
        'main.cc',
      ],
      'dependencies': [
        'buffet_common',
      ],
    },
    {
      'target_name': 'buffet_client',
      'type': 'executable',
      'sources': [
        'buffet_client.cc',
      ],
    },
    {
      'target_name': 'buffet_testrunner',
      'type': 'executable',
      'dependencies': [
        'buffet_common',
      ],
      'includes': ['../../common-mk/common_test.gypi'],
      'sources': [
        'buffet_testrunner.cc',
        'data_encoding_unittest.cc',
        'mime_utils_unittest.cc',
        'string_utils_unittest.cc',
      ],
    },
  ],
}
