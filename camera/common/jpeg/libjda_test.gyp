{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libcamera_common',
        'libcamera_ipc',
        'libjda',
        'libmojo-<(libbase_ver)',
        'libyuv',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libjda_test',
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
        'jpeg_decode_accelerator_test.cc',
      ],
    },
  ],
}
