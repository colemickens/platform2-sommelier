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
   # oobe_config library.
    {
      'target_name': 'liboobeconfig',
      'type': 'static_library',
      'dependencies': [
      ],
      'variables': {
        'gen_src_in_dir': '<(SHARED_INTERMEDIATE_DIR)/include/bindings',
        'deps': [
          'dbus-1',
        ],
      },
      'sources': [
        'oobe_config.cc',
      ],
    },
    {
      'target_name': 'oobe_config_save',
      'type': 'executable',
      'dependencies': [
        'liboobeconfig',
      ],
      'sources': [
        'oobe_config_save_main.cc',
      ],
    },
    {
      'target_name': 'oobe_config_restore',
      'type': 'executable',
      'dependencies': [
        'liboobeconfig',
      ],
      'sources': [
        'oobe_config_restore_main.cc',
      ],
    },
  ],
  # Unit tests.
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'oobe_config_test',
          'type': 'executable',
          'dependencies': [
            'liboobeconfig',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'oobe_config_test.cc',
          ],
        },
      ],
    }],
  ],
}
