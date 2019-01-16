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
      'target_name': 'libcam_algo',
      'type': 'shared_library',
      'sources': [
        'fake_libcam_algo.cc',
      ],
    },
    {
      'target_name': 'libcab_test',
      'type': 'executable',
      'includes': ['../../common-mk/common_test.gypi'],
      'variables': {
        'deps': [
          'libcab',
        ],
      },
      'libraries': [
        '-lrt',
      ],
      'sources': [
        'libcab_test_main.cc',
      ],
    },
  ],
}
