# The libpsyche package.
{
  'includes': ['common.gypi'],
  'targets': [
    {
      'target_name': 'libpsyche',
      'type': 'shared_library',
      'variables': {
        'exported_deps': [],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'lib/psyche/psyche_connection.cc',
        'lib/psyche/psyche_connection_stub.cc',
        'lib/psyche/psyche_daemon.cc',
      ],
      'dependencies': [
        'libcommon',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libpsyche_test',
          'type': 'executable',
          'includes': [ '../common-mk/common_test.gypi' ],
          'dependencies': [
            'libcommontest',
            'libpsyche',
          ],
          'sources': [
            'common/testrunner.cc',
            'lib/psyche/psyche_connection_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
