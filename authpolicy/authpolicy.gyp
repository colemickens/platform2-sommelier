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
    # Main programs.
    {
      'target_name': 'authpolicyd',
      'type': 'executable',
      'dependencies': [
        '../common-mk/external_dependencies.gyp:policy-protos',
        'user-policy-proto',
      ],
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/authpolicy',
      },
      'sources': [
        'authpolicy.cc',
        'dbus_bindings/org.chromium.AuthPolicy.xml',
        'policy/policy_keys.cc',
        'errors.cc',
        'policy/preg_parser.cc',
        'policy/registry_dict.cc',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
      'link_settings': {
        'libraries': [
          '-linstallattributes-<(libbase_ver)',
        ]
      },

    }
  ]
}
