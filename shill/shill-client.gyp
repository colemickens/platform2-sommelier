{
  'targets': [
    # shill client library generated headers. Used by other daemons to
    # interact with shill.
    {
      'target_name': 'libshill-client-headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'libshill-client-dbus-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/shill/dbus-proxies.h',
            'mock_output_file': 'include/shill/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'shill/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.flimflam.Device.dbus-xml',
            'dbus_bindings/org.chromium.flimflam.IPConfig.dbus-xml',
            'dbus_bindings/org.chromium.flimflam.Manager.dbus-xml',
            'dbus_bindings/org.chromium.flimflam.Profile.dbus-xml',
            'dbus_bindings/org.chromium.flimflam.Service.dbus-xml',
            'dbus_bindings/org.chromium.flimflam.Task.dbus-xml',
            'dbus_bindings/org.chromium.flimflam.ThirdPartyVpn.dbus-xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ]
    },
  ],
}
