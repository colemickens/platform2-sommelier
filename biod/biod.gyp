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
      'target_name': 'biod',
      'type': 'executable',
      'sources': [
        'main.cc',
        'biometrics_daemon.cc',
        'fake_biometric.cc'
      ],
    },
    {
      'target_name': 'biod_client_tool',
      'type': 'executable',
      'sources': ['tools/biod_client_tool.cc'],
    },
    {
      'target_name': 'fake_biometric_tool',
      'type': 'executable',
      'sources': ['tools/fake_biometric_tool.cc'],
    },
  ],
}
