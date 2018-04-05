{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'cros-camera-android-headers',
        'gbm',
        'libdrm',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcbm',
      'type': 'shared_library',
      'sources': [
        'camera_buffer_manager_impl.cc',
        'camera_buffer_manager_internal.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'cbm_unittest',
          'type': 'executable',
          'includes': [
            '../../../platform2/common-mk/common_test.gypi',
          ],
          'sources': [
            'camera_buffer_manager_impl.cc',
            'camera_buffer_manager_impl_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
