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
      'target_name': 'libmgmt',
      'type': 'static_library',
      'sources': [
        'policy_writer.cc',
      ],
    },
    {
      'target_name': 'policy',
      'type': 'executable',
      'dependencies': ['libmgmt'],
      'sources': [
        'main.cc',
        'policy_tool.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libmgmt_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
            'libmgmt',
          ],
          'sources': [
            'policy_tool.cc',
            'policy_tool_test.cc',
            'policy_writer_test.cc',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
        },
      ],
    }],
  ],
}
