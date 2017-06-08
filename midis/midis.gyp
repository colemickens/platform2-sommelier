{
  'targets': [
    {
      'target_name': 'midis_common',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'libudev',
        ],
        'deps': ['>@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
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
    {
      'target_name': 'libmidis',
      'type': 'static_library',
      'standalone_static_library': 1,
      'sources': [
        'libmidis/clientlib.cc',
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
            'libmidis',
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
