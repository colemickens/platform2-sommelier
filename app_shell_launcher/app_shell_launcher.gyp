{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libchromeos-ui-<(libbase_ver)',
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
