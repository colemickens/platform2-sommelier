# Caution!: GYP to GN migration is happening. If you update this file, please
# also update //camera/common/libcamera_common/BUILD.gn accordingly.

{
  'includes': ['../build/cros-camera-common.gypi'],
  'target_defaults': {
    'variables': {
      'deps': [
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_common',
      'type': 'shared_library',
      'sources': [
        'future.cc',
        'udev_watcher.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'future_unittest',
          'type': 'executable',
          'includes': [
            '../../common-mk/common_test.gypi',
          ],
          'sources': [
            'future.cc',
            'future_test.cc',
          ],
        },
      ],
    }],
  ],
}
