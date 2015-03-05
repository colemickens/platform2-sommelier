{
  'target_defaults': {
    'cflags': [
      '-Wextra',
      '-Werror',
    ],
    'cflags_cc': [
      '-fno-strict-aliasing',
      '-Woverloaded-virtual',
    ],
    'defines': [
      '__STDC_FORMAT_MACROS',
    ],
  },
  'targets': [
    {
      'target_name': 'libbrdebug',
      'type': 'static_library',
      'sources': [
        'brdebug.cc',
      ],
    },
    {
      'target_name': 'brdebugd',
      'type': 'executable',
      'sources': [
        'brdebugd.cc',
      ],
      'dependencies': [
        'libbrdebug',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'brdebug_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': ['libbrdebug'],
          'sources': [
            'brdebug_testrunner.cc',
          ],
        },
      ],
    }],
  ],
}
