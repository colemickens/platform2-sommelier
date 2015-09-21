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
    {
      'target_name': 'libshill-client-dbus-c++-proxies',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': 'dbus_bindings',
        'xml2cpp_out_dir': 'include/shill/dbus_proxies',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Device.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.IPConfig.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Manager.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Profile.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Service.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Task.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.ThirdPartyVpn.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
  ],
}
