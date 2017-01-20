{
  'targets': [
    # debugd client library generated headers. Used by other daemons to
    # interact with debugd.
    {
      'target_name': 'libdebugd-client-headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'libdebugd-client-dbus-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/debugd/dbus-proxies.h',
            'mock_output_file': 'include/debugd/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'debugd/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.debugd.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
  ],
}
