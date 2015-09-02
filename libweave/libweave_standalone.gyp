{
  'includes': [
    'libweave.gypi',
  ],
  'target_defaults': {
    'libraries': [
      '-lcrypto',
      '-lexpat',
      '-lpthread',
      '-lgtest',
      '-lgmock',
    ],
  },
  'targets': [
    {
      'target_name': 'libweave_common',
      'type': 'static_library',
      'include_dirs': [
        '../libweave/external',
      ],
      'sources': [
        '<@(weave_sources)',
        '<@(base_sources)',
      ],
    },
    {
      'target_name': 'libweave',
      'type': 'shared_library',
      'include_dirs': [
        '../libweave/external',
      ],
      'dependencies': [
        'libweave_common',
      ],
      'sources': [
        'src/empty.cc'
      ],
    },
    {
      'target_name': 'libweave-test',
      'type': 'static_library',
      'standalone_static_library': 1,
      'include_dirs': [
        '../libweave/external',
      ],
      'sources': [
        '<@(weave_test_sources)',
      ],
    },
    {
      'target_name': 'libweave_testrunner',
      'type': 'executable',
      'include_dirs': [
        '../libweave/external',
      ],
      'dependencies': [
        'libweave_common',
        'libweave-test',
      ],
      'sources': [
        '<@(weave_unittest_sources)',
        '<@(base_unittests)',
      ],
    },
    {
      'target_name': 'libweave_exports_testrunner',
      'type': 'executable',
      'include_dirs': [
        '../libweave/external',
      ],
      'dependencies': [
        'libweave',
        'libweave-test',
      ],
      'sources': [
        '<@(weave_exports_unittest_sources)',
      ],
    },
  ],
}
