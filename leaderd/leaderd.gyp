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
        'dbus_bindings/org.chromium.leaderd.Group.xml',
        'dbus_bindings/org.chromium.leaderd.Manager.xml',
        'group.cc',
        'manager.cc',
        'peerd_client.cc',
      ],
      'actions': [
        {
          #Import D - Bus bindings from peerd.
          'action_name': 'generate-peerd-proxies',
          'variables': {
            'dbus_service_config': '../peerd/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/peerd/dbus-proxies.h'
          },
          'sources': [
            '../peerd/dbus_bindings/org.chromium.peerd.Manager.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Peer.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
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
            'group_unittest.cc',
            'manager_unittest.cc',
            'leaderd_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
