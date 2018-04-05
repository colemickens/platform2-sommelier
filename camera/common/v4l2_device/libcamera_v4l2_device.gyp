{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'libcamera_v4l2_device',
      'type': 'shared_library',
      'sources': [
        'v4l2_device.cc',
        'v4l2_subdevice.cc',
        'v4l2_video_node.cc',
      ],
    },
  ],
}
