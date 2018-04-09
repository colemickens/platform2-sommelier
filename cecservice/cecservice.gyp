{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libminijail',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'cecservice',
      'type': 'executable',
      'dependencies': [
        'cecservice-adaptors',
        'libcecservice',
      ],
      'sources': [
        'main.cc',
      ],
    },
    {
      'target_name': 'libcecservice',
      'type': 'static_library',
      'sources': [
        'cecservice_dbus_adaptor.cc',
      ],
    },
    {
      'target_name': 'cecservice-adaptors',
      'type': 'none',
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/cecservice/dbus_adaptors',
      },
      'sources': [
        'dbus_bindings/org.chromium.CecService.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
  ],
}
