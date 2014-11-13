{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'cflags': [
      '-Wextra',
      '-Wno-unused-parameter',  # base/lazy_instance.h, etc.
    ],
  },
  'targets': [
    {
      'target_name': 'apmanager-adaptors',
      'type': 'none',
      'variables': {
        'generate_dbus_bindings_type': 'adaptor',
        'generate_dbus_bindings_in_dir': 'dbus_bindings',
        'generate_dbus_bindings_out_dir': 'include/apmanager/dbus_adaptors',
      },
      'sources': [
        '<(generate_dbus_bindings_in_dir)/org.chromium.apmanager.Config.xml',
        '<(generate_dbus_bindings_in_dir)/org.chromium.apmanager.Manager.xml',
        '<(generate_dbus_bindings_in_dir)/org.chromium.apmanager.Service.xml',
      ],
      'includes': ['../common-mk/generate-dbus-bindings.gypi'],
    },
    {
      'target_name': 'libapmanager',
      'type': 'static_library',
      'dependencies': [
        'apmanager-adaptors',
      ],
      'variables': {
        'exported_deps': [
          'libmetrics-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'config.cc',
        'daemon.cc',
        'manager.cc',
        'service.cc',
      ],
    },
    {
      'target_name': 'apmanagerd',
      'type': 'executable',
      'dependencies': ['libapmanager'],
      'link_settings': {
        'libraries': [
          '-lminijail',
        ],
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
          'target_name': 'apmanager_testrunner',
          'type': 'executable',
          'dependencies': ['libapmanager'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'config_unittest.cc',
            'manager_unittest.cc',
            'mock_config.cc',
            'mock_service.cc',
            'service_unittest.cc',
            'testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
