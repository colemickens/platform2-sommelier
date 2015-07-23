{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'expat',
        'openssl',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'system_api',
      ],
    },
    'include_dirs': [
      '.',
      # TODO(vitalybuka): Remove both.
      '../libweave/include',
    ],
  },
  'targets': [
    {
      'target_name': 'buffet_common',
      'type': 'static_library',
      'variables': {
        'dbus_adaptors_out_dir': 'include/buffet',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
      },
      'sources': [
        'dbus_bindings/org.chromium.Buffet.Command.xml',
        'dbus_bindings/org.chromium.Buffet.Manager.xml',
        'dbus_constants.cc',
        'manager.cc',
        '../libweave/src/base_api_handler.cc',
        '../libweave/src/buffet_config.cc',
        '../libweave/src/commands/cloud_command_proxy.cc',
        '../libweave/src/commands/command_definition.cc',
        '../libweave/src/commands/command_dictionary.cc',
        '../libweave/src/commands/command_instance.cc',
        '../libweave/src/commands/command_manager.cc',
        '../libweave/src/commands/command_queue.cc',
        '../libweave/src/commands/dbus_command_dispatcher.cc',
        '../libweave/src/commands/dbus_command_proxy.cc',
        '../libweave/src/commands/dbus_conversion.cc',
        '../libweave/src/commands/object_schema.cc',
        '../libweave/src/commands/prop_constraints.cc',
        '../libweave/src/commands/prop_types.cc',
        '../libweave/src/commands/prop_values.cc',
        '../libweave/src/commands/schema_constants.cc',
        '../libweave/src/commands/schema_utils.cc',
        '../libweave/src/commands/user_role.cc',
        '../libweave/src/device_manager.cc',
        '../libweave/src/device_registration_info.cc',
        '../libweave/src/notification/notification_parser.cc',
        '../libweave/src/notification/pull_channel.cc',
        '../libweave/src/notification/xml_node.cc',
        '../libweave/src/notification/xmpp_channel.cc',
        '../libweave/src/notification/xmpp_iq_stanza_handler.cc',
        '../libweave/src/notification/xmpp_stream_parser.cc',
        '../libweave/src/privet/ap_manager_client.cc',
        '../libweave/src/privet/cloud_delegate.cc',
        '../libweave/src/privet/constants.cc',
        '../libweave/src/privet/device_delegate.cc',
        '../libweave/src/privet/openssl_utils.cc',
        '../libweave/src/privet/peerd_client.cc',
        '../libweave/src/privet/privet_handler.cc',
        '../libweave/src/privet/privet_manager.cc',
        '../libweave/src/privet/privet_types.cc',
        '../libweave/src/privet/security_manager.cc',
        '../libweave/src/privet/shill_client.cc',
        '../libweave/src/privet/privet_types.cc',
        '../libweave/src/privet/wifi_bootstrap_manager.cc',
        '../libweave/src/privet/wifi_ssid_generator.cc',
        '../libweave/src/registration_status.cc',
        '../libweave/src/states/error_codes.cc',
        '../libweave/src/states/state_change_queue.cc',
        '../libweave/src/states/state_manager.cc',
        '../libweave/src/states/state_package.cc',
        '../libweave/src/storage_impls.cc',
        '../libweave/src/utils.cc',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
      'actions': [
        {
          'action_name': 'generate-buffet-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/buffet/dbus-proxies.h'
          },
          'sources': [
            'dbus_bindings/org.chromium.Buffet.Command.xml',
            'dbus_bindings/org.chromium.Buffet.Manager.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from peerd.
          'action_name': 'generate-peerd-proxies',
          'variables': {
            'dbus_service_config': '../peerd/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/peerd/dbus-proxies.h'
          },
          'sources': [
            '../peerd/dbus_bindings/org.chromium.peerd.Manager.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Peer.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from shill.
          'action_name': 'generate-shill-proxies',
          'variables': {
            'dbus_service_config': '../shill/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/shill/dbus-proxies.h'
          },
          'sources': [
            '../shill/dbus_bindings/org.chromium.flimflam.Device.xml',
            '../shill/dbus_bindings/org.chromium.flimflam.Manager.xml',
            '../shill/dbus_bindings/org.chromium.flimflam.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Import D-Bus bindings from apmanager.
          'action_name': 'generate-apmanager-proxies',
          'variables': {
            'dbus_service_config': '../apmanager/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/apmanager/dbus-proxies.h'
          },
          'sources': [
            '../apmanager/dbus_bindings/org.chromium.apmanager.Config.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Device.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Manager.xml',
            '../apmanager/dbus_bindings/org.chromium.apmanager.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'buffet',
      'type': 'executable',
      'dependencies': [
        'buffet_common',
      ],
      'variables': {
        'exported_deps': [
          'libwebserv-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'sources': [
        'main.cc',
      ],
    },
    {
      'target_name': 'buffet_test_daemon',
      'type': 'executable',
      'sources': [
        'test_daemon/main.cc',
      ],
    },
    {
      'target_name': 'buffet_client',
      'type': 'executable',
      'sources': [
        'buffet_client.cc',
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
            '../libweave/src/base_api_handler_unittest.cc',
            '../libweave/src/buffet_config_unittest.cc',
            '../libweave/src/buffet_testrunner.cc',
            '../libweave/src/commands/cloud_command_proxy_unittest.cc',
            '../libweave/src/commands/command_definition_unittest.cc',
            '../libweave/src/commands/command_dictionary_unittest.cc',
            '../libweave/src/commands/command_instance_unittest.cc',
            '../libweave/src/commands/command_manager_unittest.cc',
            '../libweave/src/commands/command_queue_unittest.cc',
            '../libweave/src/commands/dbus_command_dispatcher_unittest.cc',
            '../libweave/src/commands/dbus_command_proxy_unittest.cc',
            '../libweave/src/commands/dbus_conversion_unittest.cc',
            '../libweave/src/commands/object_schema_unittest.cc',
            '../libweave/src/commands/schema_utils_unittest.cc',
            '../libweave/src/commands/unittest_utils.cc',
            '../libweave/src/device_registration_info_unittest.cc',
            '../libweave/src/notification/notification_parser_unittest.cc',
            '../libweave/src/notification/xml_node_unittest.cc',
            '../libweave/src/notification/xmpp_channel_unittest.cc',
            '../libweave/src/notification/xmpp_iq_stanza_handler_unittest.cc',
            '../libweave/src/notification/xmpp_stream_parser_unittest.cc',
            '../libweave/src/privet/privet_handler_unittest.cc',
            '../libweave/src/privet/security_manager_unittest.cc',
            '../libweave/src/privet/wifi_ssid_generator_unittest.cc',
            '../libweave/src/states/state_change_queue_unittest.cc',
            '../libweave/src/states/state_manager_unittest.cc',
            '../libweave/src/states/state_package_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
