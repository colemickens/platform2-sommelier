{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'arc_camera_service_adaptors',
      'type': 'none',
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/dbus_adaptors',
        'new_fd_bindings': 1,
      },
      'sources': [
        'dbus_bindings/org.chromium.ArcCamera.xml',
      ],
      'includes': ['../../../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'arc_camera_service',
      'type': 'executable',
      'sources': [
        'arc_camera.mojom',
        'arc_camera_dbus_daemon.cc',
        'arc_camera_main.cc',
        'arc_camera_service.cc',
        'arc_camera_service_provider.cc',
        'camera_characteristics.cc',
        'ipc_util.cc',
        'v4l2_camera_device.cc',
      ],
      'includes': ['../../../common-mk/mojom_bindings_generator.gypi'],
      'dependencies': [
        'arc_camera_service_adaptors',
      ],
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libmojo-<(libbase_ver)',
        ],
        'deps': [
          '<@(exported_deps)',
          'libcamera_timezone',
        ],
        'mojo_root': '../../',
      },
    },
  ],
}
