# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'newblue',
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
        'dispatcher/catch_all_forwarder.cc',
        'dispatcher/client_manager.cc',
        'dispatcher/dbus_connection_factory.cc',
        'dispatcher/dbus_util.cc',
        'dispatcher/dispatcher.cc',
        'dispatcher/dispatcher_client.cc',
        'dispatcher/dispatcher_daemon.cc',
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
        'newblued/newblue.cc',
        'newblued/newblue_daemon.cc',
        'newblued/stack_sync_monitor.cc',
        'newblued/util.cc',
        'newblued/uuid.cc',
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
            'libnewblued',
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
            'dispatcher/catch_all_forwarder_unittest.cc',
            'dispatcher/dispatcher_client_unittest.cc',
            'dispatcher/dispatcher_unittest.cc',
            'dispatcher/impersonation_object_manager_interface_unittest.cc',
            'dispatcher/object_manager_interface_multiplexer_unittest.cc',
            'dispatcher/suspend_manager_unittest.cc',
            'dispatcher/test_helper.cc',
            'newblued/newblue_daemon_unittest.cc',
            'newblued/newblue_unittest.cc',
            'newblued/property_unittest.cc',
            'newblued/stack_sync_monitor_unittest.cc',
            'newblued/util_unittest.cc',
            'newblued/uuid_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
