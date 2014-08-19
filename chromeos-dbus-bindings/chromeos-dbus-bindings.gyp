{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'cflags': [
      '-Wextra',
    ],
    'cflags_cc': [
      '-fno-strict-aliasing',
      '-Woverloaded-virtual',
    ],
    'defines': [
      '__STDC_FORMAT_MACROS',
      '__STDC_LIMIT_MACROS',
    ],
  },
  'targets': [
    {
      'target_name': 'generate-chromeos-dbus-bindings',
      'type': 'executable',
      'sources': [
        'generate_chromeos_dbus_bindings.cc',
      ]
    },
  ],
}
