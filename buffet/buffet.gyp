{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'buffet_common',
      'type': 'static_library',
      'sources': [
        'commands/command_definition.cc',
        'commands/command_dictionary.cc',
        'commands/command_instance.cc',
        'commands/command_manager.cc',
        'commands/command_queue.cc',
        'commands/dbus_command_dispatcher.cc',
        'commands/dbus_command_proxy.cc',
        'commands/object_schema.cc',
        'commands/prop_constraints.cc',
        'commands/prop_types.cc',
        'commands/prop_values.cc',
        'commands/schema_constants.cc',
        'commands/schema_utils.cc',
        'dbus_constants.cc',
        'device_registration_info.cc',
        'manager.cc',
        'storage_impls.cc',
      ],
    },
    {
      'target_name': 'buffet',
      'type': 'executable',
      'sources': [
        'main.cc',
      ],
      'dependencies': [
        'buffet_common',
      ],
    },
    {
      'target_name': 'buffet_client',
      'type': 'executable',
      'sources': [
        'buffet_client.cc',
        'dbus_constants.cc',
      ],
      'dependencies': [
        'buffet_common',
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
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'buffet_testrunner.cc',
            'commands/command_definition_unittest.cc',
            'commands/command_dictionary_unittest.cc',
            'commands/command_instance_unittest.cc',
            'commands/command_manager_unittest.cc',
            'commands/command_queue_unittest.cc',
            'commands/dbus_command_dispatcher_unittest.cc',
            'commands/dbus_command_proxy_unittest.cc',
            'commands/object_schema_unittest.cc',
            'commands/schema_utils_unittest.cc',
            'commands/unittest_utils.cc',
            'device_registration_info_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
