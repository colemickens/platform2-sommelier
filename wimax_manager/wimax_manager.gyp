{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-c++-1',
        'glib-2.0',
        'gthread-2.0',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        # system_api depends on protobuf (or protobuf-lite). It must appear
        # before protobuf here or the linker flags won't be in the right
        # order.
        'system_api',
        'protobuf',
      ],
      # wimax_manager uses try/catch to interact with dbus-c++.
      'enable_exceptions': 1,
    },
  },
  'targets': [
    # Generated proxy headers used by wimax_manager.
    {
      'target_name': 'wimax_manager-proxies',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': 'dbus_bindings',
        'xml2cpp_out_dir': 'include/wimax_manager/dbus_proxies',
      },
      'sources': [
        # TODO(zqiu): use libpower_manger-client instead of using
        # PowerManager xml file directly.
        '../power_manager/dbus_bindings/org.chromium.PowerManager.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.DBus.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
    # wimax_manager client library generated headers. Used by other daemons to
    # interact with wimax_manager.
    {
      'target_name': 'libwimax_manager-client-headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'libwimax_manager-client-dbus-proxies',
          'variables': {
            'proxy_output_file': 'include/wimax_manager/dbus-proxies.h',
            'mock_output_file': 'include/wimax_manager/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'wimax_manager/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.WiMaxManager.Device.xml',
            'dbus_bindings/org.chromium.WiMaxManager.Network.xml',
            'dbus_bindings/org.chromium.WiMaxManager.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
  ],
  'conditions': [
    ['USE_gdmwimax == 1', {
      'targets': [
        {
          'target_name': 'wimax_manager-adaptors',
          'type': 'none',
          'variables': {
            'xml2cpp_type': 'adaptor',
            'xml2cpp_in_dir': 'dbus_bindings',
            'xml2cpp_out_dir': 'include/wimax_manager/dbus_adaptors',
          },
          'sources': [
            '<(xml2cpp_in_dir)/org.chromium.WiMaxManager.Device.xml',
            '<(xml2cpp_in_dir)/org.chromium.WiMaxManager.Network.xml',
            '<(xml2cpp_in_dir)/org.chromium.WiMaxManager.xml',
          ],
          'includes': ['../common-mk/xml2cpp.gypi'],
        },
        {
          'target_name': 'wimax_manager-protos',
          'type': 'static_library',
          'variables': {
            'proto_in_dir': 'proto',
            'proto_out_dir': 'include/wimax_manager/proto_bindings',
          },
          'sources': [
            '<(proto_in_dir)/config.proto',
            '<(proto_in_dir)/eap_parameters.proto',
            '<(proto_in_dir)/network_operator.proto',
          ],
          'includes': ['../common-mk/protoc.gypi'],
        },
        {
          'target_name': 'libwimax_manager',
          'type': 'static_library',
          'dependencies': [
            'wimax_manager-adaptors',
            'wimax_manager-proxies',
            'wimax_manager-protos',
          ],
          'link_settings': {
            'libraries': [
              # Specified on the same line to ensure they remain in this order.
              # Otherwise, the -lz gets removed if another dep already pulled it
              # in.
              '-lgdmwimax -lz',
            ],
          },
          'sources': [
            'byte_identifier.cc',
            'dbus_adaptor.cc',
            'dbus_control.cc',
            'dbus_proxy.cc',
            'dbus_service.cc',
            'dbus_service_dbus_proxy.cc',
            'device.cc',
            'device_dbus_adaptor.cc',
            'driver.cc',
            'event_dispatcher.cc',
            'gdm_device.cc',
            'gdm_driver.cc',
            'main.cc',
            'manager.cc',
            'manager_dbus_adaptor.cc',
            'network.cc',
            'network_dbus_adaptor.cc',
            'power_manager.cc',
            'power_manager_dbus_proxy.cc',
          ],
        },
        {
          'target_name': 'wimax-manager',
          'type': 'executable',
          'dependencies': ['libwimax_manager'],
          'sources': [
            'main.cc',
          ],
        },
      ],
      'conditions': [
        ['USE_test == 1', {
          'targets': [
            {
              'target_name': 'wimax_manager_testrunner',
              'type': 'executable',
              'dependencies': [
                'libwimax_manager',
                '../common-mk/testrunner.gyp:testrunner',
              ],
              'includes': ['../common-mk/common_test.gypi'],
              'sources': [
                'byte_identifier_test.cc',
                'gdm_device_test.cc',
                'manager_test.cc',
                'network_test.cc',
                'utility_test.cc',
              ],
            },
          ],
        }],
      ],
    }],
  ],
}
