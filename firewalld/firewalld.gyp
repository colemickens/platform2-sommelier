{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'dbus-glib-1',
        'glib-2.0',
        'gobject-2.0',
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
      ],
    },
    {
      'target_name': 'firewalld',
      'type': 'executable',
      'dependencies': ['libfirewalld'],
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
            'run_all_tests.cc',
          ],
        },
      ],
    }],
  ],
}
