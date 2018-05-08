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
      'target_name': 'libcommon',
      'type': 'static_library',
      'sources': [
        'common/dbus_daemon.cc',
        'common/exported_object_manager_wrapper.cc',
        'common/property.cc',
      ],
    },
    {
      'target_name': 'libdispatcher',
      'type': 'static_library',
      'sources': [
        'dispatcher/bluez_interface_handler.cc',
        'dispatcher/daemon.cc',
        'dispatcher/dbus_connection_factory.cc',
        'dispatcher/dispatcher.cc',
        'dispatcher/dispatcher_client.cc',
        'dispatcher/impersonation_object_manager_interface.cc',
        'dispatcher/object_manager_interface_multiplexer.cc',
        'dispatcher/service_watcher.cc',
        'dispatcher/suspend_manager.cc',
      ],
    },
    {
      'target_name': 'libnewblued',
      'type': 'static_library',
      'sources': [
        'newblued/newblue_daemon.cc',
      ],
    },
    {
      'target_name': 'btdispatch',
      'type': 'executable',
      'dependencies': ['libcommon', 'libdispatcher'],
      'sources': ['dispatcher/main.cc'],
    },
    {
      'target_name': 'newblued',
      'type': 'executable',
      'dependencies': ['libcommon', 'libnewblued'],
      'sources': ['newblued/main.cc'],
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
            'libcommon',
            'libdispatcher',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'common/exported_object_manager_wrapper_unittest.cc',
            'common/property_unittest.cc',
            'dispatcher/dispatcher_client_unittest.cc',
            'dispatcher/dispatcher_unittest.cc',
            'dispatcher/impersonation_object_manager_interface_unittest.cc',
            'dispatcher/object_manager_interface_multiplexer_unittest.cc',
            'dispatcher/suspend_manager_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
