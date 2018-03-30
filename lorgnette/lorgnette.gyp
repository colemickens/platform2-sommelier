{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libmetrics-<(libbase_ver)',
        'libminijail',
      ],
    },
    'cflags': [
      '-Wextra',
    ],
    'cflags_cc': [
      '-Woverloaded-virtual',
    ],
  },
  'targets': [
    {
      'target_name': 'lorgnette-adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/lorgnette/dbus_adaptors',
        'new_fd_bindings': 1,
      },
      'sources': [
        'dbus_bindings/org.chromium.lorgnette.Manager.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'liblorgnette',
      'type': 'static_library',
      'variables': {
        'dbus_adaptors_out_dir': 'include/buffet',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'exported_deps': [
          'libpermission_broker-client',
        ],
        'deps': ['>@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'dependencies': [
        'lorgnette-adaptors',
      ],
      'sources': [
        'daemon.cc',
        'epson_probe.cc',
        'firewall_manager.cc',
        'manager.cc',
      ],
    },
    {
      'target_name': 'lorgnette',
      'type': 'executable',
      'dependencies': ['liblorgnette'],
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'lorgnette_unittest',
          'type': 'executable',
          'dependencies': [
            'liblorgnette',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'manager_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
