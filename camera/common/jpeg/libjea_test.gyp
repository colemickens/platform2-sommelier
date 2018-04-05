{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libcamera_common',
        'libcamera_exif',
        'libcamera_ipc',
        'libcamera_jpeg',
        'libjea',
        'libmojo-<(libbase_ver)',
        'libyuv',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libjea_test',
      'type': 'executable',
      'includes': [
        '../../../../platform2/common-mk/common_test.gypi',
      ],
      'dependencies': [
        '../libcamera_ipc.gyp:libcamera_ipc',
      ],
      'libraries': [
        '-ljpeg',
      ],
      'sources': [
        'jpeg_encode_accelerator_test.cc',
      ],
    },
  ],
}
