# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'easy-unlock-crypto',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libeasyunlock',
      'type': 'static_library',
      'sources': [
        'dbus_adaptor.cc',
        'dbus_adaptor.h',
        'easy_unlock_service.cc',
        'easy_unlock_service.h',
      ],
      'conditions': [
        ['USE_test == 1', {
          'sources': [
            'fake_easy_unlock_service.cc',
            'fake_easy_unlock_service.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'easy_unlock',
      'type': 'executable',
      'dependencies': ['libeasyunlock'],
      'sources': ['main.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'easy_unlock_test_runner',
          'type': 'executable',
          'dependencies': [
            'libeasyunlock',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'easy_unlock_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
