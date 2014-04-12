{
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
    'include_dirs': [
      '..'  # To access all src/platform2 directories
    ],
    'cflags_cc': [
      '-std=gnu++11',
    ],
  },
  'targets': [
    {
      'target_name': 'buffet_common',
      'type': 'static_library',
      'sources': [
        'data_encoding.cc',
        'dbus_constants.cc',
        'dbus_manager.cc',
        'dbus_utils.cc',
        'exported_property_set.cc',
        'http_request.cc',
        'http_transport_curl.cc',
        'http_utils.cc',
        'manager.cc',
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
        'dbus_constants.cc',
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
        'exported_property_set_unittest.cc',
        'mime_utils_unittest.cc',
        'string_utils_unittest.cc',
      ],
    },
  ],
}
