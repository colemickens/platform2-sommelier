{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'st_flash',
      'type': 'executable',
      'sources': [
        'st_flash.cc',
      ],
    },
  ],
}
