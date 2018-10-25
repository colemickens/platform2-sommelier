{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'arc_camera_service',
      'type': 'executable',
      'sources': [
        'arc_camera.mojom',
        'arc_camera_main.cc',
        'arc_camera_service.cc',
        'arc_camera_service_provider.cc',
        'camera_characteristics.cc',
        'ipc_util.cc',
        'v4l2_camera_device.cc',
      ],
      'includes': ['../../../../platform2/common-mk/mojom_bindings_generator.gypi'],
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libmojo-<(libbase_ver)',
        ],
        'deps': ['<@(exported_deps)'],
        'mojo_root': '../../',
      },
    },
  ],
}
