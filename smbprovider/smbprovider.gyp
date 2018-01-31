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
        'iterator/depth_first_iterator.cc',
        'iterator/depth_first_iterator.h',
        'iterator/directory_iterator.cc',
        'iterator/directory_iterator.h',
        'mount_manager.cc',
        'mount_manager.h',
        'proto.cc',
        'proto.h',
        'samba_interface.h',
        'samba_interface_impl.cc',
        'samba_interface_impl.h',
        'smbprovider.cc',
        'smbprovider.h',
        'smbprovider_helper.cc',
        'smbprovider_helper.h',
        'temp_file_manager.cc',
        'temp_file_manager.h',
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
            'fake_samba_test.cc',
            'iterator/depth_first_iterator_test.cc',
            'iterator/directory_iterator_test.cc',
            'mount_manager_test.cc',
            'proto_test.cc',
            'smbprovider_helper_test.cc',
            'smbprovider_test.cc',
            'smbprovider_test_helper.cc',
            'smbprovider_test_helper.h',
            'temp_file_manager_test.cc',
          ],
        },
      ],
    }],
  ],
}
