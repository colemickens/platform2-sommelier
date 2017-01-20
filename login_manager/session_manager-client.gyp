{
  'targets': [
    # session_manager client library generated headers. Used by other daemons to
    # interact with session_manager.
    {
      'target_name': 'libsession_manager-client-headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'libsession_manager-client-dbus-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/session_manager/dbus-proxies.h',
            'mock_output_file': 'include/session_manager/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'session_manager/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.SessionManagerInterface.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
  ],
}
