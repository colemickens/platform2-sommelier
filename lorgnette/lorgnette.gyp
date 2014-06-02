{
  'target_defaults': {
    'dependencies': [
      '../../platform2/libchromeos/libchromeos-<(libbase_ver).gyp:libchromeos-<(libbase_ver)',
    ],
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
    'cflags': [
      '-Wextra',
      '-Wno-missing-field-initializers',  # for LAZY_INSTANCE_INITIALIZER
      '-Wno-unused-parameter',  # base/lazy_instance.h, etc.
    ],
    'cflags_cc': [
      '-std=gnu++11',
      '-Woverloaded-virtual',
    ],
  },
  'targets': [
    {
      'target_name': 'lorgnette-adaptors',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_in_dir': 'dbus_bindings',
        'xml2cpp_out_dir': 'include/lorgnette/dbus_adaptors',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.chromium.lorgnette.Manager.xml',
      ],
      'includes': ['../../platform2/common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'lorgnette-proxies',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': 'dbus_bindings',
        'xml2cpp_out_dir': 'include/lorgnette/dbus_proxies',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.chromium.lorgnette.Manager.xml',
      ],
      'includes': ['../../platform2/common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'liblorgnette',
      'type': 'static_library',
      'dependencies': [
        '../metrics/libmetrics-<(libbase_ver).gyp:libmetrics-<(libbase_ver)',
        'lorgnette-adaptors',
        'lorgnette-proxies',
      ],
      'variables': {
        'exported_deps': [
          'dbus-c++-1',
          'gio-2.0',
          'glib-2.0',
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
      'link_settings': {
        'variables': {
          'deps': [
            'dbus-c++-1',
            'gio-2.0',
            'glib-2.0',
          ],
        },
        'libraries': [
          '-lminijail',
          '-lrt'
        ],
      },
      'sources': [
        'daemon.cc',
        'manager.cc',
        'minijail.cc',
      ],
    },
    {
      'target_name': 'lorgnette',
      'type': 'executable',
      'dependencies': ['liblorgnette'],
      'sources': [
        'main.cc',
      ]
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'lorgnette_unittest',
          'type': 'executable',
          'dependencies': ['liblorgnette'],
          'includes': ['../../platform2/common-mk/common_test.gypi'],
          'sources': [
            'manager_unittest.cc',
            'testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
