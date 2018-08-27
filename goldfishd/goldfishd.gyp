{
  'target_defaults' : {
    'variables' : {
      'deps' : [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },

  'targets': [
    {
      'target_name': 'libgoldfishd',
      'type': 'static_library',
      'sources': [
        'goldfish_library.cc',
      ],
    },
    {
      'target_name': 'goldfishd',
      'type': 'executable',
      'dependencies': ['libgoldfishd'],
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'goldfishd_test_runner',
          'type': 'executable',
          'dependencies': [
            'libgoldfishd',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'goldfish_library_test.cc',
          ],
        },
      ],
    }],
  ],
}
