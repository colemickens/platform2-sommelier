{
  'target_defaults': {
    'defines': [
      '__STDC_FORMAT_MACROS',
    ],
  },
  'targets': [
    {
      'target_name': 'libsoma',
      'type': 'static_library',
      'sources': [
        'soma.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'soma_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'dependencies': ['libsoma'],
          'sources': [
            'soma_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
