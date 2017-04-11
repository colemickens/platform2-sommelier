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
      'target_name': 'midis',
      'type': 'executable',
      'link_settings': {
        'libraries': [
          '-ldl',
        ],
      },
      'sources': [
        'client.cc',
        'client_tracker.cc',
        'daemon.cc',
        'device.cc',
        'device_tracker.cc',
        'file_handler.cc',
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'device_tracker_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'device.cc',
            'device_tracker.cc',
            'device_tracker_test.cc',
            'file_handler.cc',
          ],
        },
        {
          'target_name': 'udev_handler_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'device.cc',
            'device_tracker.cc',
            'file_handler.cc',
            'udev_handler_test.cc',
          ],
        },
        {
          'target_name': 'device_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'device.cc',
            'device_test.cc',
            'file_handler.cc',
          ],
        },
      ],
    }],
  ],
}
