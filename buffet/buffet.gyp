{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libchrome-<(libbase_ver)',
        'libchrome-test-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libcurl',
        'libmetrics-<(libbase_ver)',
      ],
    },
    'link_settings': {
      'libraries': [
        '-lbase-dbus_test_support-<(libbase_ver)',
      ],
    },
    'cflags_cc': [
      '-std=gnu++11',
    ],
  },
  'targets': [
    {
      'target_name': 'buffet_common',
      'type': 'static_library',
      'sources': [
        'any.cc',
        'commands/command_definition.cc',
        'commands/command_dictionary.cc',
        'commands/command_instance.cc',
        'commands/command_manager.cc',
        'commands/command_queue.cc',
        'commands/object_schema.cc',
        'commands/prop_constraints.cc',
        'commands/prop_types.cc',
        'commands/prop_values.cc',
        'commands/schema_constants.cc',
        'commands/schema_utils.cc',
        'data_encoding.cc',
        'dbus_constants.cc',
        'dbus_utils.cc',
        'device_registration_info.cc',
        'error.cc',
        'error_codes.cc',
        'exported_object_manager.cc',
        'http_request.cc',
        'http_connection_curl.cc',
        'http_transport_curl.cc',
        'http_utils.cc',
        'manager.cc',
        'mime_utils.cc',
        'storage_impls.cc',
        'string_utils.cc',
        'url_utils.cc'
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
    {
      'target_name': 'buffet_testrunner',
      'type': 'executable',
      'dependencies': [
        'buffet_common',
      ],
      'includes': ['../common-mk/common_test.gypi'],
      'sources': [
        'any_unittest.cc',
        'any_internal_impl_unittest.cc',
        'buffet_testrunner.cc',
        'commands/command_definition_unittest.cc',
        'commands/command_dictionary_unittest.cc',
        'commands/command_instance_unittest.cc',
        'commands/command_manager_unittest.cc',
        'commands/command_queue_unittest.cc',
        'commands/object_schema_unittest.cc',
        'commands/schema_utils_unittest.cc',
        'commands/unittest_utils.cc',
        'data_encoding_unittest.cc',
        'device_registration_info_unittest.cc',
        'error_unittest.cc',
        'exported_object_manager_unittest.cc',
        'http_connection_fake.cc',
        'http_transport_fake.cc',
        'http_utils_unittest.cc',
        'mime_utils_unittest.cc',
        'string_utils_unittest.cc',
        'url_utils_unittest.cc'
      ],
    },
  ],
}
