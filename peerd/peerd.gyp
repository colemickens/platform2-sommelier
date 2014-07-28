{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'link_settings': {
      'libraries': [
      ],
    },
  },
  'targets': [
    {
      'target_name': 'peerd_common',
      'type': 'static_library',
      'sources': [
        'manager.cc',
        'dbus_constants.cc',
      ],
    },
    {
      'target_name': 'peerd',
      'type': 'executable',
      'sources': [
        'main.cc',
      ],
      'dependencies': [
        'peerd_common',
      ],
    },
    {
      'target_name': 'peerd_testrunner',
      'type': 'executable',
      'dependencies': [
        'peerd_common',
      ],
      'variables': {
        'deps': [
          'libchrome-test-<(libbase_ver)',
        ],
      },
      'includes': ['../common-mk/common_test.gypi'],
      'sources': [
        'manager_unittest.cc',
        'peerd_testrunner.cc',
      ],
    },
  ],
}
