{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'dbus-c++-1',
        'protobuf-lite',
        'libbrillo-<(libbase_ver)',
        'libcap',
        'libchrome-<(libbase_ver)',
        'libminijail',
      ],
    },
  },
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
      'sources': [
        'authpolicy.cc',
        'policy/device_policy_encoder.cc',
        'policy/policy_encoder_helper.cc',
        'policy/policy_keys.cc',
        'policy/preg_parser.cc',
        'policy/preg_policy_encoder.cc',
        'policy/registry_dict.cc',
        'policy/user_policy_encoder.cc',
        'policy/user_policy_encoder_gen.cc',
        'pipe_helper.cc',
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
      'sources': [
        'authpolicy_parser_main.cc',
      ],
    },
    # Authpolicy daemon executable.
    {
      'target_name': 'authpolicyd',
      'type': 'executable',
      'dependencies': ['libauthpolicy'],
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
          'sources': [
            'authpolicy_testrunner.cc',
            'process_executor_unittest.cc',
            'samba_interface_internal_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
