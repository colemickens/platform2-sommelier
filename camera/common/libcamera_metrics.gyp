{
  'includes': [
    '../build/cros-camera-common.gypi',
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
      'target_name': 'libcamera_metrics',
      'includes': [
        '../build/standalone_static_library.gypi',
      ],
      'libraries': [
        '-lmetrics-<(libbase_ver)',
      ],
      'direct_dependent_settings': {
        'libraries': [
          '-lmetrics-<(libbase_ver)',
        ],
      },
      'sources': [
        'camera_metrics_impl.cc',
      ],
    },
  ],
}
