{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'generate_camera_profile',
      'type': 'executable',
      'sources': [
        'generate_camera_profile.cc',
      ],
    },
  ],
}
