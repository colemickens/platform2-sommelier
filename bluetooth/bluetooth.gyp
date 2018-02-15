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
      'target_name': 'libdispatcher',
      'type': 'static_library',
      'sources': [
        'dispatcher/daemon.cc',
        'dispatcher/exported_object_manager_wrapper.cc',
        'dispatcher/object_manager_interface_multiplexer.cc',
        'dispatcher/property.cc',
        'dispatcher/service_watcher.cc',
        'dispatcher/suspend_manager.cc',
      ],
    },
    {
      'target_name': 'btdispatch',
      'type': 'executable',
      'dependencies': ['libdispatcher'],
      'sources': ['dispatcher/main.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'btdispatch_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libdispatcher',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'dispatcher/exported_object_manager_wrapper_unittest.cc',
            'dispatcher/object_manager_interface_multiplexer_unittest.cc',
            'dispatcher/property_unittest.cc',
            'dispatcher/suspend_manager_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
