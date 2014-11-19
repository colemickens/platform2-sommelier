{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'openssl',
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
        'device_delegate.cc',
        'privet_handler.cc',
        'security_delegate.cc',
        'wifi_delegate.cc',
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
            'security_delegate_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
