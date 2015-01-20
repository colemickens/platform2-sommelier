{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'leaderd_common',
      'type': 'static_library',
      'variables': {
        'dbus_adaptors_out_dir': 'include/leaderd',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
      },
      'sources': [
        'manager.cc',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'leaderd',
      'type': 'executable',
      'sources': [
        'main.cc',
      ],
      'dependencies': [
        'leaderd_common',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'leaderd_testrunner',
          'type': 'executable',
          'dependencies': [
            'leaderd_common',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'manager_unittest.cc',
            'leaderd_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
