{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'cros-camera-android-headers',
        'libcamera_metadata',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_hal',
      'type': 'shared_library',
      'sources': [
        'camera_device.cc',
        'camera_hal.cc',
        'ip_camera_detector.cc',
        'metadata_handler.cc',
        'mock_frame_generator.cc',
        'request_queue.cc',
      ],
    },
  ],
}
