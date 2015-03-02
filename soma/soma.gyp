{
  'target_defaults': {
    'defines': [
      '__STDC_FORMAT_MACROS',
    ],
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libsoma',
      'type': 'static_library',
      'sources': [
        'container_spec.cc',
        'device_filter.cc',
        'namespace.cc',
        'port.cc',
        'spec_reader.cc',
        'sysfs_filter.cc',
        'soma.cc',
        'usb_device_filter.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'soma_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'dependencies': ['libsoma'],
          'sources': [
            'container_spec_unittest.cc',
            'soma_testrunner.cc',
            'spec_reader_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
