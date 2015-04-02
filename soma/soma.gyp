{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
    'include_dirs': [
      'lib',
    ],
  },
  'targets': [
    {
      'target_name': 'soma-proto-lib',
      'type': 'static_library',
      'variables': {
        'exported_deps': [
          'libprotobinder',
          'protobuf-lite',
        ],
        'proto_in_dir': 'idl',
        'proto_out_dir': 'include/soma/proto_bindings',
        'gen_bidl': 1,
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        '<(proto_in_dir)/soma_container_spec.proto',
        '<(proto_in_dir)/soma.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libsomad',
      'type': 'static_library',
      'dependencies': [
        'soma-proto-lib',
      ],
      'sources': [
        'container_spec_wrapper.cc',
        'device_filter.cc',
        'namespace.cc',
        'port.cc',
        'service_name.cc',
        'spec_reader.cc',
        'sysfs_filter.cc',
        'usb_device_filter.cc',
      ],
    },
    {
      'target_name': 'libsoma',
      'type': 'shared_library',
      'dependencies': [
        'soma-proto-lib',
      ],
      'sources': [
        'lib/soma/read_only_container_spec.cc',
      ],
    },
    {
      'target_name': 'somad',
      'type': 'executable',
      'dependencies': [
        'libsomad',
      ],
      'sources': [
        'main.cc',
        'soma.cc',
      ],
      'variables': {
        'deps': [
          'libpsyche',
        ],
      },
    },
    {
      'target_name': 'soma_client',
      'type': 'executable',
      'dependencies': [
        'libsoma',
        'soma-proto-lib',
      ],
      'sources': ['soma_client.cc'],
      'variables': {
        'deps': [
          'libpsyche',
        ],
      },
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'somad_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'dependencies': [
            'libsomad',
          ],
          'sources': [
            'container_spec_unittest.cc',
            'soma_testrunner.cc',
            'spec_reader_unittest.cc',
          ],
        },
        {
          'target_name': 'libsoma_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'dependencies': [
            'libsoma',
            'soma-proto-lib',
          ],
          'sources': [
            'lib/soma/libsoma_testrunner.cc',
            'lib/soma/read_only_container_spec_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
