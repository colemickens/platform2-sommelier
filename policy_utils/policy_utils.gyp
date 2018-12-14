{
  'targets': [
    {
      'target_name': 'libmgmt',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
        'deps': [
          '<@(exported_deps)',
        ],
      },
      'sources': [
        'policy_writer.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libmgmt_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
            'libmgmt',
          ],
          'sources': [
            'policy_writer_test.cc',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
        },
      ],
    }],
  ],
}
