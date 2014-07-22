{
  'target_defaults': {
    'variables': {
      'deps': [
        'easy-unlock-crypto',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'cflags_cc': [
      '-std=gnu++11',
    ],
  },
  'targets': [
    {
      'target_name': 'libeasyunlock',
      'type': 'static_library',
      'sources': [
        'daemon.cc',
        'daemon.h',
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
    }
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'easy_unlock_test_runner',
          'type': 'executable',
          'dependencies': ['libeasyunlock'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'easy_unlock_unittest.cc',
            'test_runner.cc',
          ],
        }
      ],
    }],
  ],
}
