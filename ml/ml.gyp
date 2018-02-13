{
  'targets': [
    {
      'target_name': 'ml_service',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        'daemon.cc',
        'main.cc',
      ],
    },
  ],
}
