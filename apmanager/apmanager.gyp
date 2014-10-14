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
        '<(generate_dbus_bindings_in_dir)/org.chromium.apmanager.Manager.xml',
      ],
      'includes': ['../common-mk/generate-dbus-bindings.gypi'],
    },
    {
      'target_name': 'apmanager-proxies',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': 'dbus_bindings',
        'xml2cpp_out_dir': 'include/apmanager/dbus_proxies',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.chromium.apmanager.Manager.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'libapmanager',
      'type': 'static_library',
      'dependencies': [
        'apmanager-adaptors',
        'apmanager-proxies',
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
        'daemon.cc',
        'manager.cc',
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
            'manager_unittest.cc',
            'testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
