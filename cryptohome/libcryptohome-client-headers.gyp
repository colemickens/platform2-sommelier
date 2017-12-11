{
  'targets': [
    # cryptohome client library generated headers. Used by other tools to
    # interact with cryptohome.
    {
      'target_name': 'libcryptohome-client-headers',
      'type': 'none',
      'actions': [
        {
          'action_name': 'libcryptohome-client-dbus-proxies',
          'variables': {
            'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
            'proxy_output_file': 'include/cryptohome/dbus-proxies.h',
            'mock_output_file': 'include/cryptohome/dbus-proxy-mocks.h',
            'proxy_path_in_mocks': 'cryptohome/dbus-proxies.h',
          },
          'sources': [
            'dbus_bindings/org.chromium.CryptohomeInterface.xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
  ],
}
