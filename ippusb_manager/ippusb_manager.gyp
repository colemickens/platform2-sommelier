{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libminijail',
        'libusb-1.0',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'ippusb_manager',
      'type': 'executable',
      'sources': [
        'ippusb_manager.cc',
        'socket_connection.cc',
        'usb.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'ippusb_manager_testrunner',
          'type': 'executable',
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'usb.cc',
            'usb_test.cc',
          ],
        },
      ],
    }],
  ],
}
