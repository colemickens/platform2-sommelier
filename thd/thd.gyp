{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libthd',
      'type': 'static_library',
      'sources': [
        'mechanism/fake_mechanism.cc',
        'mechanism/file_write_mechanism.cc',
        'mechanism/mechanism.cc',
        'source/ectool_temps_source.cc',
        'source/fake_source.cc',
        'source/file_source.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'thd_unittests',
          'type': 'executable',
          'dependencies': [
            'libthd',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'mechanism/file_write_mechanism_test.cc',
            'source/file_source_test.cc',
          ],
        },
      ],
    }],
  ],
}
