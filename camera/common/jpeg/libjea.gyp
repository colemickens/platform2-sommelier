{
  'includes': [
    '../../build/cros-camera-common.gypi',
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
      'target_name': 'libjea',
      'includes': [
        '../../build/standalone_static_library.gypi',
      ],
      'dependencies': [
        '../libcamera_ipc.gyp:libcamera_ipc',
      ],
      'sources': [
        'jpeg_encode_accelerator_impl.cc',
      ],
    },
    {
      'target_name': 'copy_libjea',
      'includes': [
        '../../build/file_copy.gypi',
      ],
      'variables': {
        'src': '<(PRODUCT_DIR)/libjea.a',
        'dst': '<(PRODUCT_DIR)/libjea.pic.a',
      },
    },
  ],
}
