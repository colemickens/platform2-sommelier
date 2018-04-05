{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libcamera_common',
        'libcamera_metadata',
        'libmojo-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_ipc',
      'includes': [
        '../build/standalone_static_library.gypi',
        '../mojo/mojom_bindings_gen.gypi',
      ],
      'sources': [
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
        'camera_mojo_channel_manager_impl.cc',
        'ipc_util.cc',
      ],
    },
    {
      'target_name': 'copy_libcamera_ipc',
      'includes': [
        '../build/file_copy.gypi',
      ],
      'variables': {
        'src': '<(PRODUCT_DIR)/libcamera_ipc.a',
        'dst': '<(PRODUCT_DIR)/libcamera_ipc.pic.a',
      },
    },
  ],
}
