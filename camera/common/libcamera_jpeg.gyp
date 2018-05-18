{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libmojo-<(libbase_ver)',
        'libyuv',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_jpeg',
      'type': 'shared_library',
      'dependencies': [
        'jpeg/libjea.gyp:libjea',
      ],
      'sources': [
        'jpeg_compressor_impl.cc',
      ],
    },
  ],
}
