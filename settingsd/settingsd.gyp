{
  'target_defaults': {
    'include_dirs': ['.'],
  },
  'targets': [
    {
      'target_name': 'settingsd_common',
      'type': 'static_library',
      'sources': [
      ],
    },
    {
      'target_name': 'settingsd_testrunner',
      'type': 'executable',
      'dependencies': [
        'settingsd_common',
      ],
      'includes': ['../common-mk/common_test.gypi'],
      'sources': [
        'settingsd_testrunner.cc',
      ],
    }
  ],
}
