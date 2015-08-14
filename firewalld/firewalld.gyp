{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libpermission_broker-client',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libfirewalld',
      'type': 'static_library',
      'sources': [
        'firewall_daemon.cc',
        'firewall_service.cc',
        'iptables.cc',
      ],
    },
    {
      'target_name': 'firewalld-dbus-adaptor',
      'type': 'none',
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/firewalld/dbus_adaptor',
      },
      'sources': [
        'dbus_bindings/org.chromium.Firewalld.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'firewalld-dbus-proxies',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate-firewalld-dbus-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/firewalld/dbus-proxies.h',
            'mock_output_file': 'include/firewalld/dbus-mocks.h',
            'proxy_path_in_mocks': 'firewalld/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.Firewalld.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'firewalld',
      'type': 'executable',
      'dependencies': [
        'libfirewalld',
        'firewalld-dbus-adaptor',
      ],
      'sources': ['main.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'firewalld_unittest',
          'type': 'executable',
          'variables': {
            'deps': [
              'libpermission_broker-client-test',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['libfirewalld'],
          'sources': [
            'iptables_unittest.cc',
            'mock_iptables.cc',
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
