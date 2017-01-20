{
  'targets': [
    # power_manager client library generated headers. Used by other daemons to
    # interact with power_manager.
    {
      'target_name': 'libpower_manager-client-headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'libpower_manager-client-dbus-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/power_manager/dbus-proxies.h',
            'mock_output_file': 'include/power_manager/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'power_manager/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.PowerManager.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
  ],
}
