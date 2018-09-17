{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libpermission_broker-client',
      ],
    },
    'include_dirs': ['.'],
  },
  'targets': [
    {
      'target_name': 'libwebserv_common',
      'type': 'static_library',
      'variables': {
        # Not using dbus_service_config here deliberately in order not to
        # get tied to some constant service name, since it will be
        # provided by the consumer of libwebserv library.
        'dbus_service_config': '',
        'dbus_adaptors_out_dir': 'include/dbus_bindings',
        'dbus_xml_extension': 'dbus-xml',
        'new_fd_bindings': 1,
      },
      # This static library is used in libwebserv shared library, which means
      # we must generate position-independent code for the files comprising
      # this library. Since this option is disabled by default for targets
      # other than 'shared_library', turn it on explicitly for this lib.
      # Turn off the default -fPIE flag (which is set for static_library
      # in ../common-mk/common.gypi) and replace it with -fPIC.
      'cflags!': ['-fPIE'],
      'cflags': ['-fPIC'],
      'includes': [
        '../common-mk/generate-dbus-adaptors.gypi',
      ],
      'sources': [
        'libwebserv/dbus_bindings/org.chromium.WebServer.RequestHandler.dbus-xml',
        'libwebserv/dbus_protocol_handler.cc',
        'libwebserv/dbus_response.cc',
        'libwebserv/dbus_server.cc',
        'libwebserv/protocol_handler.cc',
        'libwebserv/request.cc',
        'libwebserv/request_handler_callback.cc',
        'libwebserv/request_utils.cc',
        'libwebserv/response.cc',
        'libwebserv/server.cc',
      ],
      'actions': [
        {
          'action_name': 'generate-webservd-proxies',
          'variables': {
            'dbus_service_config': 'webservd/dbus_bindings/dbus-service-config.json',
            'mock_output_file': 'include/webservd/dbus-mocks.h',
            'proxy_output_file': 'include/webservd/dbus-proxies.h',
            'dbus_adaptors_out_dir': '',
          },
          'sources': [
            'webservd/dbus_bindings/org.chromium.WebServer.ProtocolHandler.dbus-xml',
            'webservd/dbus_bindings/org.chromium.WebServer.Server.dbus-xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'webservd_common',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libmicrohttpd',
          'openssl',
        ],
        'deps': ['<@(exported_deps)'],
        'dbus_adaptors_out_dir': 'include/dbus_bindings',
        'dbus_service_config': 'webservd/dbus_bindings/dbus-service-config.json',
        'dbus_xml_extension': 'dbus-xml',
        'new_fd_bindings': 1,
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        'webservd/config.cc',
        'webservd/dbus_bindings/org.chromium.WebServer.ProtocolHandler.dbus-xml',
        'webservd/dbus_bindings/org.chromium.WebServer.Server.dbus-xml',
        'webservd/dbus_protocol_handler.cc',
        'webservd/dbus_request_handler.cc',
        'webservd/error_codes.cc',
        'webservd/fake_encryptor.cc',
        'webservd/log_manager.cc',
        'webservd/permission_broker_firewall.cc',
        'webservd/protocol_handler.cc',
        'webservd/request.cc',
        'webservd/server.cc',
        'webservd/temp_file_manager.cc',
        'webservd/utils.cc',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
      'actions': [
        {
          'action_name': 'generate-libwebserv-proxies',
          'variables': {
            # Not using dbus_service_config here deliberately in order not to
            # get tied to some constant service name, since it will be
            # provided by the consumer of libwebserv library.
            'dbus_service_config': '',
            'mock_output_file': 'include/libwebserv/dbus-mocks.h',
            'proxy_output_file': 'include/libwebserv/dbus-proxies.h',
            'new_fd_bindings': 1,
          },
          'sources': [
            'libwebserv/dbus_bindings/org.chromium.WebServer.RequestHandler.dbus-xml',
          ],
          'includes': ['../common-mk/generate-dbus-proxies.gypi'],
        },
      ],
    },
    {
      'target_name': 'libwebserv-<(libbase_ver)',
      'type': 'shared_library',
      'includes': [
        '../common-mk/deps.gypi',
      ],
      'dependencies': [
        'libwebserv_common',
      ],
      'sources': [
        'libwebserv/_empty.cc',
      ],
    },
    {
      'target_name': 'webservd',
      'type': 'executable',
      'dependencies': [
        'webservd_common',
      ],
      'variables': {
        'deps': [
          'libminijail',
        ],
      },
      'sources': [
        'webservd/main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libwebserv_testrunner',
          'type': 'executable',
          'dependencies': [
            'libwebserv_common',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'libwebserv/libwebserv_testrunner.cc',
          ],
        },
        {
          'target_name': 'webservd_testrunner',
          'type': 'executable',
          'dependencies': [
            'webservd_common',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'webservd/config_test.cc',
            'webservd/log_manager_test.cc',
          ],
        },
      ],
    }],
  ],
}
