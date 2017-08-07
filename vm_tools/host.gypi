{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'maitred_client',
      'type': 'executable',
      'dependencies': ['vm-rpcs'],
      'sources': [
        'maitred/client.cc',
      ],
    },
  ],
}
