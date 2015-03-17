{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'cflags_cc': [
      '-fno-strict-aliasing',
      '-Woverloaded-virtual',
    ],
    'defines': [
      '__STDC_FORMAT_MACROS',
    ],
  },
  'targets': [
    {
      'target_name': 'libbrdebug',
      'type': 'static_library',
      'sources': [
        'device_property_watcher.cc',
        'peerd_client.cc',
      ],
      'actions': [
        {
          # Import D-Bus bindings from peerd.
          'action_name': 'generate-peerd-proxies',
          'variables': {
            'dbus_service_config': '../peerd/dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/peerd/dbus-proxies.h',
          },
          'sources': [
            '../peerd/dbus_bindings/org.chromium.peerd.Manager.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Peer.xml',
            '../peerd/dbus_bindings/org.chromium.peerd.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'brdebugd',
      'type': 'executable',
      'sources': [
        'brdebugd.cc',
      ],
      'dependencies': [
        'libbrdebug',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'brdebug_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['libbrdebug'],
          'sources': [
            'brdebug_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
