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
      'sources': [
        'avahi_client.cc',
        'avahi_service_publisher.cc',
        'dbus_constants.cc',
        'dbus_data_serialization.cc',
        'discovered_peer.cc',
        'ip_addr.cc',
        'manager.cc',
        'peer.cc',
        'peer_manager_impl.cc',
        'published_peer.cc',
        'service.cc',
        'technologies.cc',
        'typedefs.cc',
      ],
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
            'dbus_data_serialization_unittest.cc',
            'discovered_peer_unittest.cc',
            'manager_unittest.cc',
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
