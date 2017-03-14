{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'midis',
      'type': 'executable',
      'link_settings': {
        'libraries': [
          '-ldl',
        ],
      },
      'sources': [
        'daemon.cc',
        'device_tracker.cc',
        'main.cc',
      ],
    },
  ],
}
