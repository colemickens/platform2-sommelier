{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'system_api',
      ],
    },
    'include_dirs': [
      '.',
    ],
  },
  'targets': [
    {
      'target_name': 'buffet_common',
      'type': 'static_library',
      'variables': {
        'dbus_adaptors_out_dir': 'include/buffet',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'exported_deps': [
          'libwebserv-<(libbase_ver)',
          'libweave-<(libbase_ver)',
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
        'ap_manager_client.cc',
        'dbus_bindings/org.chromium.Buffet.Command.xml',
        'dbus_bindings/org.chromium.Buffet.Manager.xml',
        'dbus_command_dispatcher.cc',
        'dbus_command_proxy.cc',
        'dbus_conversion.cc',
        'dbus_constants.cc',
        'manager.cc',
        'peerd_client.cc',
        'shill_client.cc',
        'webserv_client.cc',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
      'actions': [
        {
          'action_name': 'generate-buffet-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/buffet/dbus-proxies.h'
          },
          'sources': [
            'dbus_bindings/org.chromium.Buffet.Command.xml',
            'dbus_bindings/org.chromium.Buffet.Manager.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from peerd.
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
        {
          # Import D-Bus bindings from shill.
          'action_name': 'generate-shill-proxies',
          'variables': {
            'dbus_service_config': '../shill/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/shill/dbus-proxies.h'
          },
          'sources': [
            '../shill/dbus_bindings/org.chromium.flimflam.Device.xml',
            '../shill/dbus_bindings/org.chromium.flimflam.Manager.xml',
            '../shill/dbus_bindings/org.chromium.flimflam.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from apmanager.
          'action_name': 'generate-apmanager-proxies',
          'variables': {
            'dbus_service_config': '../apmanager/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/apmanager/dbus-proxies.h'
          },
          'sources': [
            '../apmanager/dbus_bindings/org.chromium.apmanager.Config.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Device.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Manager.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'buffet',
      'type': 'executable',
      'dependencies': [
        'buffet_common',
      ],
      'sources': [
        'main.cc',
      ],
    },
    {
      'target_name': 'buffet_test_daemon',
      'type': 'executable',
      'sources': [
        'test_daemon/main.cc',
      ],
    },
    {
      'target_name': 'buffet_client',
      'type': 'executable',
      'sources': [
        'buffet_client.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'buffet_testrunner',
          'type': 'executable',
          'dependencies': [
            'buffet_common',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
              'libchromeos-test-<(libbase_ver)',
              'libweave-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'buffet_testrunner.cc',
            'dbus_command_proxy_unittest.cc',
            'dbus_conversion_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
