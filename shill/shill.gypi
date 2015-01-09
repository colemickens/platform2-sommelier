{
  'targets': [
    {
      'target_name': 'shill-proxies',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': 'dbus_bindings',
        'xml2cpp_out_dir': 'include/shill/dbus_proxies',
      },
      'sources': [
        '<(xml2cpp_in_dir)/dbus-properties.xml',
        '<(xml2cpp_in_dir)/dbus-service.xml',
        '<(xml2cpp_in_dir)/dhcpcd.xml',
        '<(xml2cpp_in_dir)/power_manager.xml',
        '<(xml2cpp_in_dir)/supplicant-bss.xml',
        '<(xml2cpp_in_dir)/supplicant-interface.xml',
        '<(xml2cpp_in_dir)/supplicant-network.xml',
        '<(xml2cpp_in_dir)/supplicant-process.xml',
        '<(xml2cpp_in_dir)/upstart.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Device.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.IPConfig.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Manager.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Profile.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Service.xml',
        '<(xml2cpp_in_dir)/org.chromium.flimflam.Task.xml',
      ],
      'conditions': [
        ['USE_cellular == 1', {
          'sources': [
            '<(xml2cpp_in_dir)/dbus-objectmanager.xml',
            '<(xml2cpp_in_dir)/modem-gobi.xml',
          ],
        }],
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
  ]
}
