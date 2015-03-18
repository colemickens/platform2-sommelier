{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libprotobinder',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libgerm',
      'type': 'static_library',
      'sources': [
        'daemon.cc',
        'environment.cc',
        'launcher.cc',
      ],
    },
    {
      'target_name': 'germ',
      'type': 'executable',
      'dependencies': [
        'libgerm',
      ],
      'sources': ['germ_main.cc'],
    },
    {
      'target_name': 'germd',
      'type': 'executable',
      'dependencies': [
        'libgerm',
      ],
      'sources': ['germd.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'germ_unittest',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['libgerm'],
          'sources': [
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
