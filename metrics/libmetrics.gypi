# TODO: Fix the visibility on these shared libs.
# gyplint: disable=GypLintVisibilityFlags

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libsession_manager-client',
      ],
    },
    'cflags_cc': [
      '-fno-exceptions',
    ],
  },
  'targets': [
    {
      'target_name': 'libmetrics-<(libbase_ver)',
      'type': 'shared_library',
      'cflags': [
        '-fvisibility=default',
      ],
      'libraries+': [
        '-lpolicy-<(libbase_ver)',
      ],
      'sources': [
        'c_metrics_library.cc',
        'cumulative_metrics.cc',
        'metrics_library.cc',
        'persistent_integer.cc',
        'serialization/metric_sample.cc',
        'serialization/serialization_utils.cc',
        'timer.cc',
      ],
      'include_dirs': ['.'],
      'defines': [ 'USE_METRICS_UPLOADER=<(USE_metrics_uploader)' ],
    },
  ],
}
