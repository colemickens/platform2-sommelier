{
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'testrunner',
          'type': 'static_library',
          'includes': ['common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
