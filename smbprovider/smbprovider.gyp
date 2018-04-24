{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libpasswordprovider',
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
        'new_fd_bindings': 1,
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
        'credential_store.cc',
        'credential_store.h',
        'in_memory_credential_store.cc',
        'in_memory_credential_store.h',
        'iterator/depth_first_iterator.cc',
        'iterator/depth_first_iterator.h',
        'iterator/directory_iterator.cc',
        'iterator/directory_iterator.h',
        'iterator/post_depth_first_iterator.cc',
        'iterator/post_depth_first_iterator.h',
        'iterator/pre_depth_first_iterator.cc',
        'iterator/pre_depth_first_iterator.h',
        'iterator/share_iterator.h',
        'kerberos_artifact_client.cc',
        'kerberos_artifact_client.h',
        'kerberos_artifact_client_interface.h',
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
            'credential_store_test.cc',
            'fake_samba_interface.cc',
            'fake_samba_interface.h',
            'fake_samba_test.cc',
            'in_memory_credential_store_test.cc',
            'iterator/depth_first_iterator_test.cc',
            'iterator/directory_iterator_test.cc',
            'iterator/post_depth_first_iterator_test.cc',
            'iterator/pre_depth_first_iterator_test.cc',
            'iterator/share_iterator_test.cc',
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
