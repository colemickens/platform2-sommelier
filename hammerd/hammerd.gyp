{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libbrillo-<(libbase_ver)',
        'libusb-1.0',
        'fmap',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libhammerd',
      'type': 'static_library',
      'sources': [
        'fmap_utils.cc',
        'hammer_updater.cc',
        'process_lock.cc',
        'update_fw.cc',
        'usb_utils.cc',
      ],
    },
    {
      'target_name': 'hammerd',
      'type': 'executable',
      'dependencies': ['libhammerd'],
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'unittest_runner',
          'type': 'executable',
          'dependencies': [
            'libhammerd',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'update_fw_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
