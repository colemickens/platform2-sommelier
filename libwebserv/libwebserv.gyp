{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'include_dirs': ['.'],
  },
  'targets': [
    {
      'target_name': 'libwebserv-<(libbase_ver)',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [
          'libmicrohttpd',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'includes': ['../common-mk/deps.gypi'],
      'sources': [
        'connection.cc',
        'response.cc',
        'request.cc',
        'request_handler_callback.cc',
        'server.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libwebserv_testrunner',
          'type': 'executable',
          'dependencies': [
            'libwebserv-<(libbase_ver)',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'libwebserv_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
