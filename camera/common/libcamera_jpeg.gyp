{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libyuv',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_jpeg',
      'type': 'shared_library',
      'sources': [
        'jpeg_compressor.cc',
      ],
    },
  ],
}
