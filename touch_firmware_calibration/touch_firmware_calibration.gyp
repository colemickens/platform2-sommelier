{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },

  'targets': [
    {
      'target_name': 'override-max-pressure',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
        ],
      },
      'sources': [
        'override_max_pressure.cc',
      ],
    },
  ],
}
