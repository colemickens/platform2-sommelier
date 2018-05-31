{
  'target_defaults': {
    'defines': [
      'FUSE_USE_VERSION=26',
    ],
    'variables': {
      'deps': [
        'fuse',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'arc-appfuse-provider-adaptors',
      'type': 'none',
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/appfuse/dbus_adaptors',
        'new_fd_bindings': 1,
      },
      'sources': [
        'dbus_bindings/org.chromium.ArcAppfuseProvider.xml',
      ],
      'includes': ['../../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'arc-appfuse-provider',
      'type': 'executable',
      'dependencies': [
        'arc-appfuse-provider-adaptors',
      ],
      'sources': [
        'appfuse_mount.cc',
        'arc_appfuse_provider.cc',
        'data_filter.cc',
      ],
    },
  ],
}
