{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets' : [
    {
      'target_name' : 'hermes',
      'type' : 'executable',
      'sources' : [
        'esim_uim_impl.cc',
        'lpd.cc',
        'main.cc',
        'smdp_fi_impl.cc',
      ],
    },
    {
      'target_name' : 'libhermes',
      'type' : 'static_library',
      'sources' : [
        'esim_uim_impl.cc',
        'lpd.cc',
        'smdp_fi_impl.cc',
      ],
    },
  ],
  'conditions' : [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'hermes_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libhermes',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'esim_uim_unittest.cc',
            'smdp_fi_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
