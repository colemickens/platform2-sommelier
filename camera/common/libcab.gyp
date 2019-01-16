{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libcamera_common',
        'libmojo-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'cros_camera_algo',
      'type': 'executable',
      'variables': {
        'mojo_root': '../',
      },
      'includes': [
        '../../common-mk/mojom_bindings_generator.gypi',
      ],
      'libraries': [
        '-ldl',
      ],
      'sources': [
        '../mojo/algorithm/camera_algorithm.mojom',
        'camera_algorithm_adapter.cc',
        'camera_algorithm_main.cc',
        'camera_algorithm_ops_impl.cc',
        'ipc_util.cc',
      ],
    },
    {
      'target_name': 'libcab',
      'includes': [
        '../build/standalone_static_library.gypi',
      ],
      'dependencies': [
        'libcamera_ipc.gyp:libcamera_ipc',
      ],
      'sources': [
        'camera_algorithm_bridge_impl.cc',
        'camera_algorithm_callback_ops_impl.cc',
      ],
    },
    {
      'target_name': 'copy_libcab',
      'includes': [
        '../build/file_copy.gypi',
      ],
      'variables': {
        'src': '<(PRODUCT_DIR)/libcab.a',
        'dst': '<(PRODUCT_DIR)/libcab.pic.a',
      },
    },
  ],
}
