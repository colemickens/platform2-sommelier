{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'dbus-c++-1',
        'protobuf-lite',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    # User policy proto compiler.
    {
      'target_name': 'user-policy-proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'policy/proto',
        'proto_out_dir': 'include/authpolicy/policy/proto',
      },
      'sources': [
        '<(proto_in_dir)/cloud_policy.proto',
      ],
     'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
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
    # Authpolicy library.
    {
      'target_name': 'libauthpolicy',
      'type': 'static_library',
      'dependencies': [
        '../common-mk/external_dependencies.gyp:policy-protos',
        'user-policy-proto',
        'dbus_code_generator',
      ],
      'sources': [
        'authpolicy.cc',
        'errors.cc',
        'policy/device_policy_encoder.cc',
        'policy/policy_encoder_helper.cc',
        'policy/policy_keys.cc',
        'policy/preg_parser.cc',
        'policy/preg_policy_encoder.cc',
        'policy/registry_dict.cc',
        'policy/user_policy_encoder.cc',
        'policy/user_policy_encoder_gen.cc',
        'process_executor.cc',
      ],
    },
    # Main program.
    {
      'target_name': 'authpolicyd',
      'type': 'executable',
      'dependencies': ['libauthpolicy'],
      'sources': ['authpolicy_main.cc'],
      'link_settings': {
        'libraries': [
          '-linstallattributes-<(libbase_ver)',
        ]
      },
    }
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
          ],
        },
      ],
    }],
  ],
}
