{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'smbclient',
        # system_api depends on protobuf (or protobuf-lite). It must
        # appear before protobuf or the linker flags won't be in the right
        # order.
        'system_api',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'smbproviderd_adaptors',
      'type': 'none',
      'variables': {
        'dbus_adaptors_out_dir': 'include/dbus_adaptors',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
      },
      'sources': [
        'dbus_bindings/org.chromium.SmbProvider.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'libsmbprovider',
      'type': 'static_library',
      'dependencies': [
        'smbproviderd_adaptors',
      ],
      'sources': [
        'constants.cc',
        'constants.h',
        'samba_interface.h',
        'samba_interface_impl.cc',
        'samba_interface_impl.h',
        'smbprovider.cc',
        'smbprovider.h',
        'smbprovider_helper.cc',
        'smbprovider_helper.h',
      ],
    },
    {
      'target_name': 'smbproviderd',
      'type': 'executable',
      'dependencies': [
        'libsmbprovider',
      ],
      'sources': [
        'smbprovider_main.cc',
      ],
    },
  ],
  # Unit tests.
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'smbprovider_test',
          'type': 'executable',
          'dependencies': [
            'libsmbprovider',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'fake_samba_interface.cc',
            'fake_samba_interface.h',
            'smbprovider_helper_test.cc',
            'smbprovider_test.cc',
          ],
        },
      ],
    }],
  ],
}
