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
    },
    {
      'target_name': 'firewalld-dbus-adaptor',
      'type': 'none',
      'variables': {
        'dbus_adaptors_type': 'adaptor',
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
        'firewalld-dbus-adaptor'
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
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
