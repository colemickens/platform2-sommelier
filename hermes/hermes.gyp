# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
    'link_settings': {
      'libraries': [
        '-lqrtr',
      ],
    },
  },
  'targets' : [
    {
      'target_name' : 'libhermes',
      'type' : 'static_library',
      'sources' : [
        'esim_qmi_impl.cc',
        'lpd.cc',
        'qmi_uim.c',
        'smdp_impl.cc',
      ],
    },
    {
      'target_name' : 'hermes',
      'type' : 'executable',
      'dependencies': [
        'libhermes',
      ],
      'sources' : [
        'main.cc',
      ],
    },
  ],
  'conditions' : [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'hermes_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'dependencies': [
            'libhermes',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'esim_qmi_unittest.cc',
            'smdp_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
