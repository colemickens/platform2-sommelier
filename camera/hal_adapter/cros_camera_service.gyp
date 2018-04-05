{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'cros-camera-android-headers',
        'libbrillo-<(libbase_ver)',
        'libcamera_common',
        'libcamera_metadata',
        'libdrm',
        'libmojo-<(libbase_ver)',
        'libsync',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'cros_camera_service',
      'type': 'executable',
      'includes': [
        '../mojo/mojom_bindings_gen.gypi',
      ],
      'libraries': [
        '-ldl',
      ],
      'sources': [
        '../common/ipc_util.cc',
        '../common/utils/camera_hal_enumerator.cc',
        '../mojo/algorithm/camera_algorithm.mojom',
        '../mojo/camera3.mojom',
        '../mojo/camera_common.mojom',
        '../mojo/camera_metadata.mojom',
        '../mojo/camera_metadata_tags.mojom',
        '../mojo/cros_camera_service.mojom',
        '../mojo/jda/geometry.mojom',
        '../mojo/jda/jpeg_decode_accelerator.mojom',
        '../mojo/jda/time.mojom',
        '../mojo/jea/jpeg_encode_accelerator.mojom',
        'camera3_callback_ops_delegate.cc',
        'camera3_device_ops_delegate.cc',
        'camera_device_adapter.cc',
        'camera_hal_adapter.cc',
        'camera_hal_server_impl.cc',
        'camera_module_callbacks_delegate.cc',
        'camera_module_delegate.cc',
        'cros_camera_main.cc',
        'cros_camera_mojo_utils.cc',
      ],
    },
  ],
}
