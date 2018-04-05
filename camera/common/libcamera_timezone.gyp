{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'libcamera_timezone',
      'type': 'shared_library',
      'sources': [
        'timezone.cc',
      ],
    },
  ],
}
