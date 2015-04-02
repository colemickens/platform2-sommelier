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
        'lib/psyche/psyche_daemon.cc',
      ],
      'dependencies': [
        'libcommon',
      ],
    },
  ],
}
