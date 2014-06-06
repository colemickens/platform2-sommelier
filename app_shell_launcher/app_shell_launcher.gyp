{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'app_shell_launcher',
      'type': 'executable',
      'sources': ['app_shell_launcher.cc'],
    },
  ],
}
