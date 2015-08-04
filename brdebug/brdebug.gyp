{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libpeerd-client',
      ],
    },
    'cflags_cc': [
      '-fno-strict-aliasing',
      '-Woverloaded-virtual',
    ],
  },
  'targets': [
    {
      'target_name': 'libbrdebug',
      'type': 'static_library',
      'sources': [
        'device_property_watcher.cc',
        'peerd_client.cc',
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
