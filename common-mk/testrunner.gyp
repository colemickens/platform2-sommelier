# Caution!: GYP to GN migration is happening. If you update this file, please
# update common-mk/testrunner/BUILD.gn too accordingly.
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
