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
      'target_name': 'libsecure_erase_file',
      'type': 'shared_library',
      'sources': [
        'secure_erase_file.cc',
      ],
    },
    {
      'target_name': 'secure_erase_file',
      'type': 'executable',
      'sources': [
        'secure_erase_file_main.cc',
      ],
      'dependencies': [
        'libsecure_erase_file',
      ],
    },
  ],
}
