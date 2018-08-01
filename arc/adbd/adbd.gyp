{
  'targets': [
    {
      'target_name': 'arc-adbd',
      'type': 'executable',
      'sources': [
        'adbd.cc',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
    },
  ],
}
