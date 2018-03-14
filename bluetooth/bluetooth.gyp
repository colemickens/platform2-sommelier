{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        # system_api depends on protobuf (or protobuf-lite). It must appear
        # before protobuf here or the linker flags won't be in the right
        # order.
        'system_api',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libnewblued',
      'type': 'static_library',
      'sources': [
        'newblued/daemon.cc',
        'newblued/service_watcher.cc',
        'newblued/suspend_manager.cc',
      ],
    },
    {
      'target_name': 'newblued',
      'type': 'executable',
      'dependencies': ['libnewblued'],
      'sources': ['newblued/main.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'newblued_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libnewblued',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'newblued/suspend_manager_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
