# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
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
        'dbus_bindings/org.chromium.peerd.Manager.xml',
        'dbus_bindings/org.chromium.peerd.Peer.xml',
        'dbus_bindings/org.chromium.peerd.Service.xml',
        'dbus_constants.cc',
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
      'actions': [
        {
          # Import D-Bus bindings from Avahi.
          'action_name': 'generate-avahi-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/avahi-service-config.json',
            'proxy_output_file': 'include/avahi/dbus-proxies.h',
            'shared_interface_dir': '<(sysroot)/usr/share/dbus-1/interfaces',
          },
          'sources': [
            '<(shared_interface_dir)/org.freedesktop.Avahi.EntryGroup.xml',
            '<(shared_interface_dir)/org.freedesktop.Avahi.Server.xml',
            '<(shared_interface_dir)/org.freedesktop.Avahi.ServiceBrowser.xml',
            '<(shared_interface_dir)/org.freedesktop.Avahi.ServiceResolver.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
        {
          # Generate D-Bus proxies for peerd.
          'action_name': 'generate-peerd-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/peerd/dbus-proxies.h',
            'mock_output_file': 'include/peerd/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'peerd/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.peerd.Manager.xml',
            'dbus_bindings/org.chromium.peerd.Peer.xml',
            'dbus_bindings/org.chromium.peerd.Service.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
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
            '../common-mk/testrunner.gyp:testrunner',
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
            'service_unittest.cc',
            'test_util.cc',
          ],
        },
      ],
    }],
  ],
}
