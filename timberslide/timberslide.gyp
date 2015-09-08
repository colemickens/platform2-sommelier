{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libbrillo-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'timberslide',
      'type': 'executable',
      'sources': [
        'timberslide.cc',
      ],
    },
  ],
}
