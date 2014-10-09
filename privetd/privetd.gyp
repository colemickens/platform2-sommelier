{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'privetd_common',
      'type': 'static_library',
      'sources': [
        'cloud_delegate.cc',
        'cloud_delegate.h',
        'device_delegate.cc',
        'device_delegate.h',
        'privet_handler.cc',
        'privet_handler.h',
        'privet_handler.h',
        'security_delegate.cc',
        'security_delegate.h',
        'wifi_delegate.cc',
        'wifi_delegate.h',
      ],
    },
    {
      'target_name': 'privetd',
      'type': 'executable',
      'dependencies': [
        'privetd_common',
      ],
      'variables': {
        'exported_deps': [
          'libwebserv-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'privetd_testrunner',
          'type': 'executable',
          'dependencies': [
            'privetd_common',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'privetd_testrunner.cc',
            'privet_handler_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
