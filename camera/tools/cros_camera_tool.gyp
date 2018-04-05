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
      'target_name': 'cros-camera-tool',
      'type': 'executable',
      'sources': [
        'cros_camera_tool.cc',
      ],
    },
  ],
}
