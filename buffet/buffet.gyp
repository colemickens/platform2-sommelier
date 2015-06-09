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
    'include_dirs': ['.'],
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
        'base_api_handler.cc',
        'buffet_config.cc',
        'commands/command_definition.cc',
        'commands/command_dictionary.cc',
        'commands/command_instance.cc',
        'commands/command_manager.cc',
        'commands/command_queue.cc',
        'commands/dbus_command_dispatcher.cc',
        'commands/dbus_command_proxy.cc',
        'commands/cloud_command_proxy.cc',
        'commands/object_schema.cc',
        'commands/prop_constraints.cc',
        'commands/prop_types.cc',
        'commands/prop_values.cc',
        'commands/schema_constants.cc',
        'commands/schema_utils.cc',
        'commands/user_role.cc',
        'device_registration_info.cc',
        'dbus_bindings/org.chromium.Buffet.Command.xml',
        'dbus_bindings/org.chromium.Buffet.Manager.xml',
        'dbus_constants.cc',
        'manager.cc',
        'notification/notification_parser.cc',
        'notification/pull_channel.cc',
        'notification/xml_node.cc',
        'notification/xmpp_channel.cc',
        'notification/xmpp_iq_stanza_handler.cc',
        'notification/xmpp_stream_parser.cc',
        'privet/ap_manager_client.cc',
        'privet/cloud_delegate.cc',
        'privet/constants.cc',
        'privet/daemon_state.cc',
        'privet/device_delegate.cc',
        'privet/main.cc',
        'privet/openssl_utils.cc',
        'privet/peerd_client.cc',
        'privet/privet_handler.cc',
        'privet/privetd_conf_parser.cc',
        'privet/security_manager.cc',
        'privet/shill_client.cc',
        'privet/wifi_bootstrap_manager.cc',
        'privet/wifi_ssid_generator.cc',
        'registration_status.cc',
        'storage_impls.cc',
        'states/error_codes.cc',
        'states/state_change_queue.cc',
        'states/state_manager.cc',
        'states/state_package.cc',
        'utils.cc',
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
            'base_api_handler_unittest.cc',
            'buffet_testrunner.cc',
            'buffet_config_unittest.cc',
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
            'notification/notification_parser_unittest.cc',
            'notification/xml_node_unittest.cc',
            'notification/xmpp_channel_unittest.cc',
            'notification/xmpp_iq_stanza_handler_unittest.cc',
            'notification/xmpp_stream_parser_unittest.cc',
            'privet/privetd_conf_parser_unittest.cc',
            'privet/privet_handler_unittest.cc',
            'privet/security_manager_unittest.cc',
            'privet/wifi_ssid_generator_unittest.cc',
            'states/state_change_queue_unittest.cc',
            'states/state_manager_unittest.cc',
            'states/state_package_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
