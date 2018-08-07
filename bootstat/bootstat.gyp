# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'targets': [
    {
      'target_name': 'libbootstat',
      'type': 'shared_library',
      'sources': [
        'bootstat_log.c',
      ],
      'libraries': [
        '-lrootdev',
      ],
    },
    {
      'target_name': 'bootstat',
      'type': 'executable',
      'sources': [
        'bootstat.c',
      ],
      'dependencies': [
        'libbootstat',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libbootstat_unittests',
          'type': 'executable',
          'dependencies': [
            'libbootstat',
          ],
          'includes': [
            '../common-mk/common_test.gypi',
          ],
          'sources': [
            'log_unit_tests.cc',
          ],
        },
      ],
    }],
  ],
}
