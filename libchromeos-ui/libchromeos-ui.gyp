{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'include_dirs': [
      '.',
    ],
  },
  'targets': [
    {
      'target_name': 'libchromeos-ui-<(libbase_ver)',
      'type': 'shared_library',
      'libraries': [
        '-lbootstat',
      ],
      'cflags': [
        '-fvisibility=default',
      ],
      'sources': [
        'chromeos/ui/chromium_command_builder.cc',
        'chromeos/ui/util.cc',
        'chromeos/ui/x_server_runner.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libchromeos-ui-test',
          'type': 'executable',
          'dependencies': [
            'libchromeos-ui-<(libbase_ver)',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'chromeos/ui/chromium_command_builder_unittest.cc',
            'chromeos/ui/x_server_runner_unittest.cc',
            'testrunner.cc',
          ],
        },
      ]},
    ],
  ],
}
