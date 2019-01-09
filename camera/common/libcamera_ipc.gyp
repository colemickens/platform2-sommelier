# Caution!: GYP to GN migration is happening. If you update this file, please
# also update camera/common/libcamera_ipc/BUILD.gn accordingly.
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
      'variables': {
        'mojo_root': '../',
      },
      'includes': [
        '../build/standalone_static_library.gypi',
        '../../common-mk/mojom_bindings_generator.gypi',
      ],
      'sources': [
        '../mojo/algorithm/camera_algorithm.mojom',
        '../mojo/camera3.mojom',
        '../mojo/camera_common.mojom',
        '../mojo/camera_metadata.mojom',
        '../mojo/camera_metadata_tags.mojom',
        '../mojo/cros_camera_service.mojom',
        '../mojo/gpu/dmabuf.mojom',
        '../mojo/gpu/jpeg_encode_accelerator.mojom',
        '../mojo/gpu/mjpeg_decode_accelerator.mojom',
        '../mojo/gpu/time.mojom',
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
