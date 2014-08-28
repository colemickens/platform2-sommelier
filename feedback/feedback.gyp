{
  'variables': {
    'libbase_ver': 271506,
  },
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libcurl',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libfeedback_daemon',
      'type': 'static_library',
      'dependencies': ['../common-mk/external_dependencies.gyp:feedback-protos'],
      'sources': [
        'feedback_service.cc',
        'feedback_uploader_curl.cc',
        'feedback_util.cc',
      ],
    },
    {
      'target_name': 'feedback_daemon',
      'type': 'executable',
      'dependencies': ['libfeedback_daemon', ],
      'sources': [
        'feedback_daemon.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'feedback_daemon_test',
          'type': 'executable',
          'dependencies': [
            'libfeedback_daemon',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'feedback_service_unittest.cc',
          ]
        },
      ],
    }],
  ],
}
