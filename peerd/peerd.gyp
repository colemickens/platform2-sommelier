{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'peerd_common',
      'type': 'static_library',
      'variables': {
        'dbus_adaptors_out_dir': 'include/peerd',
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
      },
      'sources': [
        'avahi_client.cc',
        'avahi_service_discoverer.cc',
        'avahi_service_publisher.cc',
        'constants.cc',
        'dbus_constants.cc',
        'dbus_bindings/org.chromium.peerd.Manager.xml',
        'dbus_bindings/org.chromium.peerd.Peer.xml',
        'dbus_bindings/org.chromium.peerd.Service.xml',
        'discovered_peer.cc',
        'manager.cc',
        'peer.cc',
        'peer_manager_impl.cc',
        'published_peer.cc',
        'service.cc',
        'technologies.cc',
        'typedefs.cc',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'peerd',
      'type': 'executable',
      'sources': [
        'main.cc',
      ],
      'dependencies': [
        'peerd_common',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'peerd_testrunner',
          'type': 'executable',
          'dependencies': [
            'peerd_common',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'avahi_client_unittest.cc',
            'avahi_service_publisher_unittest.cc',
            'discovered_peer_unittest.cc',
            'manager_unittest.cc',
            'peer_manager_impl_unittest.cc',
            'peer_unittest.cc',
            'published_peer_unittest.cc',
            'peerd_testrunner.cc',
            'service_unittest.cc',
            'test_util.cc',
          ],
        },
      ],
    }],
  ],
}
