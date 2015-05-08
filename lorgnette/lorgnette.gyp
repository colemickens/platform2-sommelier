{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libmetrics-<(libbase_ver)',
      ],
    },
    'cflags': [
      '-Wextra',
      '-Wno-unused-parameter',  # base/lazy_instance.h, etc.
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
      },
      'sources': [
        'dbus_bindings/org.chromium.lorgnette.Manager.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'liblorgnette',
      'type': 'static_library',
      'dependencies': [
        'lorgnette-adaptors',
      ],
      'link_settings': {
        'libraries': [
          '-lminijail',
        ],
      },
      'sources': [
        'daemon.cc',
        'firewall_manager.cc',
        'manager.cc',
      ],
      'actions': [
        {
          'action_name': 'generate-permission_broker-proxies',
          'variables': {
            'proxy_output_file': 'include/permission_broker/dbus-proxies.h'
          },
          'sources': [
            '../permission_broker/dbus_bindings/org.chromium.PermissionBroker.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
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
