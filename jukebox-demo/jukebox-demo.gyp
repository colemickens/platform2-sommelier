{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'jukebox-demo',
      'type': 'executable',
      'actions': [
        {
          'action_name': 'generate-buffet-proxies',
          'variables': {
            'dbus_service_config': '../buffet/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/buffet/dbus-proxies.h'
          },
          'sources': [
            '../buffet/dbus_bindings/org.chromium.Buffet.Command.xml',
            '../buffet/dbus_bindings/org.chromium.Buffet.Manager.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
      'sources': [
        'main.cc',
      ],
    },
  ],
}
