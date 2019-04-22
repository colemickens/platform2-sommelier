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
      'target_name': 'libjda',
      'includes': [
        '../../build/standalone_static_library.gypi',
      ],
      'dependencies': [
        '../libcamera_ipc.gyp:libcamera_ipc',
        '../libcamera_metrics.gyp:libcamera_metrics',
      ],
      'sources': [
        'jpeg_decode_accelerator_impl.cc',
      ],
    },
    {
      'target_name': 'copy_libjda',
      'includes': [
        '../../build/file_copy.gypi',
      ],
      'variables': {
        'src': '<(PRODUCT_DIR)/libjda.a',
        'dst': '<(PRODUCT_DIR)/libjda.pic.a',
      },
    },
  ],
}
