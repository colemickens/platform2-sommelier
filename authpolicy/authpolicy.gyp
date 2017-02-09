{
  'targets': [
    # D-Bus code generator.
    {
      'target_name': 'dbus_code_generator',
      'type': 'none',
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/authpolicy',
      },
      'sources': [
        'dbus_bindings/org.chromium.AuthPolicy.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    # Container protos
    {
      'target_name': 'container-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'include/bindings',
      },
      'sources': [
        '<(proto_in_dir)/authpolicy_containers.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    # Authpolicy library.
    {
      'target_name': 'libauthpolicy',
      'type': 'static_library',
      'dependencies': [
        '../common-mk/external_dependencies.gyp:policy-protos',
        '../common-mk/external_dependencies.gyp:user_policy-protos',
        'dbus_code_generator',
        'container-protos',
      ],
      'variables': {
        'deps': [
          'dbus-1',
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
      'sources': [
        'authpolicy.cc',
        'authpolicy_metrics.cc',
        'path_service.cc',
        'policy/device_policy_encoder.cc',
        'policy/policy_encoder_helper.cc',
        'policy/policy_keys.cc',
        'policy/preg_policy_encoder.cc',
        'policy/user_policy_encoder.cc',
        'policy/user_policy_encoder_gen.cc',
        'platform_helper.cc',
        'process_executor.cc',
        'samba_interface.cc',
        'samba_interface_internal.cc',
      ],
    },
    # Parser tool.
    {
      'target_name': 'authpolicy_parser',
      'type': 'executable',
      'dependencies': ['libauthpolicy'],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libcap',
          'libchrome-<(libbase_ver)',
          'protobuf-lite',
        ],
      },
      'sources': [
        'authpolicy_parser_main.cc',
      ],
    },
    # Authpolicy daemon executable.
    {
      'target_name': 'authpolicyd',
      'type': 'executable',
      'dependencies': [
        'libauthpolicy',
        'authpolicy_parser',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libcap',
          'libchrome-<(libbase_ver)',
          'libmetrics-<(libbase_ver)',
          'libminijail',
          'protobuf-lite',
        ],
      },
      'sources': ['authpolicy_main.cc'],
      'link_settings': {
        'libraries': [
          '-linstallattributes-<(libbase_ver)',
        ],
      },
    },
  ],
  # Unit tests.
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'authpolicy_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'dependencies': ['libauthpolicy'],
          'variables': {
            'deps': [
              'libbrillo-<(libbase_ver)',
              'libcap',
              'libchrome-<(libbase_ver)',
              'libchrome-test-<(libbase_ver)',
              'libmetrics-<(libbase_ver)',
              'libminijail',
              'protobuf-lite',
            ],
          },
          'sources': [
            'authpolicy_testrunner.cc',
            'authpolicy_unittest.cc',
            'process_executor_unittest.cc',
            'samba_interface_internal_unittest.cc',
          ],
        },
        {
          'target_name': 'stub_common',
          'type': 'static_library',
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
            ],
          },
          'sources': ['stub_common.cc'],
        },
        {
          'target_name': 'stub_net',
          'type': 'executable',
          'dependencies': [
            'libauthpolicy',
            'stub_common',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
            ],
          },
          'sources': ['stub_net_main.cc'],
        },
        {
          'target_name': 'stub_kinit',
          'type': 'executable',
          'dependencies': [
            'libauthpolicy',
            'stub_common',
          ],
          'variables': {
            'deps': [
              'libchrome-<(libbase_ver)',
            ],
          },
          'sources': ['stub_kinit_main.cc'],
        },
      ],
    }],
  ],
}
