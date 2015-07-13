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
      'target_name': 'libfirewalld',
      'type': 'static_library',
      'sources': [
        'firewall_daemon.cc',
        'firewall_service.cc',
        'iptables.cc',
      ],
      'actions': [
        {
          'action_name': 'generate-permission_broker-proxies',
          'variables': {
            'dbus_service_config': '<(platform2_root)/permission_broker/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/permission_broker/dbus-proxies.h',
          },
          'sources': [
            '<(platform2_root)/permission_broker/dbus_bindings/org.chromium.PermissionBroker.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
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
