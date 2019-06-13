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
        '../common/utils/camera_config_impl.cc',
        'generate_camera_profile.cc',
      ],
    },
  ],
}
