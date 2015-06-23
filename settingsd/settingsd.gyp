{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
    'include_dirs': ['.'],
  },
  'targets': [
    {
      'target_name': 'settingsd_common',
      'type': 'static_library',
      'sources': [
        'identifier_utils.h',
        'key.cc',
        'key.h',
        'settings_document.h',
        'settings_map.h',
        'simple_settings_map.cc',
        'simple_settings_map.h',
        'version_stamp.cc',
        'version_stamp.h',
      ],
    },
    {
      'target_name': 'settingsd_testrunner',
      'type': 'executable',
      'dependencies': [
        'settingsd_common',
      ],
      'includes': ['../common-mk/common_test.gypi'],
      'sources':[
       'identifier_utils_unittest.cc',
        'key_unittest.cc',
        'mock_settings_document.cc',
        'mock_settings_document.h',
        'settingsd_testrunner.cc',
        'simple_settings_map_unittest.cc',
        'test_helpers.cc',
        'test_helpers.h',
        'version_stamp_unittest.cc',
      ],
    }
  ],
}
