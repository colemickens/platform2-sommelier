# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libminijail',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'cecservice',
      'type': 'executable',
      'dependencies': [
        'cecservice-adaptors',
        'libcecservice',
      ],
      'sources': [
        'main.cc',
      ],
    },
    {
      'target_name': 'libcecservice',
      'type': 'static_library',
      'sources': [
        'cec_device.cc',
        'cec_fd.cc',
        'cec_manager.cc',
        'cecservice_dbus_adaptor.cc',
        'udev.cc',
      ],
    },
    {
      'target_name': 'cecservice-adaptors',
      'type': 'none',
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/cecservice/dbus_adaptors',
      },
      'sources': [
        'dbus_bindings/org.chromium.CecService.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'cecservice_testrunner',
          'type': 'executable',
          'dependencies': [
            'libcecservice',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'cec_device_unittest.cc',
            'cec_manager_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
