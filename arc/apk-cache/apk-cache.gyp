{
  'targets': [
    {
      'target_name': 'libapk-cache-cleaner',
      'type': 'static_library',
      'sources': [
        'cache_cleaner.cc',
      ],
      'variables': {
        'deps': [
          'libchrome-<(libbase_ver)',
        ],
      },
    },
    {
      'target_name': 'apk-cache-cleaner',
      'type': 'executable',
      'sources': [
        'cache_cleaner_main.cc',
      ],
      'dependencies': [
        'libapk-cache-cleaner',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'apk-cache-cleaner_testrunner',
          'type': 'executable',
          'dependencies': [
            'libapk-cache-cleaner',
            '../../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'cache_cleaner_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
