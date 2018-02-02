{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'cros-camera-android-headers',
        'libcamera_jpeg',
        'libcbm',
        'libcamera_client',
        'libcamera_common',
        'libcamera_exif',
        'libcamera_metadata',
        'libcamera_timezone',
        'libjda',
        'libjea',
        'libsync',
        'libyuv',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_hal',
      'type': 'shared_library',
      'sources': [
        'cached_frame.cc',
        'camera_characteristics.cc',
        'camera_client.cc',
        'camera_hal.cc',
        'camera_hal_device_ops.cc',
        'capture_request.cc',
        'frame_buffer.cc',
        'image_processor.cc',
        'metadata_handler.cc',
        'stream_format.cc',
        'test_pattern.cc',
        'v4l2_camera_device.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'image_processor_unittest',
          'type': 'executable',
          'includes': [
            '../../../../platform2/common-mk/common_test.gypi',
          ],
          'sources': [
            'frame_buffer.cc',
            'image_processor.cc',
            'unittest/image_processor_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
