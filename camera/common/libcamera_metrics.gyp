{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'libcamera_metrics',
      'includes': [
        '../build/standalone_static_library.gypi',
      ],
      'libraries': [
        '-lmetrics-<(libbase_ver)',
      ],
      'sources': [
        'camera_metrics_impl.cc',
      ],
    },
  ],
}
