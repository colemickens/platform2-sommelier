{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'midis_common',
      'type': 'static_library',
      'sources': [
        'client.cc',
        'client_tracker.cc',
        'device.cc',
        'device_tracker.cc',
        'file_handler.cc',
        'subdevice_client_fd_holder.cc',
      ],
    },
    {
      'target_name': 'midis',
      'type': 'executable',
      'dependencies' : [
        'midis_common',
      ],
      'link_settings': {
        'libraries': [
          '-ldl',
        ],
      },
      'sources': [
        'daemon.cc',
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'midis_testrunner',
          'type': 'executable',
          'dependencies' : [
            '../common-mk/testrunner.gyp:testrunner',
            'midis_common',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'tests/client_test.cc',
            'tests/client_tracker_test.cc',
            'tests/device_test.cc',
            'tests/device_tracker_test.cc',
            'tests/test_helper.cc',
            'tests/udev_handler_test.cc',
          ],
        },
      ],
    }],
  ],
}
