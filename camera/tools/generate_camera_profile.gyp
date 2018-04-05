{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libcamera_timezone',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'generate_camera_profile',
      'type': 'executable',
      'sources': [
        '../hal/usb/camera_characteristics.cc',
        '../hal/usb/v4l2_camera_device.cc',
        'generate_camera_profile.cc',
      ],
    },
  ],
}
