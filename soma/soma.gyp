{
  'target_defaults': {
    'defines': [
      '__STDC_FORMAT_MACROS',
    ],
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
        'libprotobinder',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'soma-proto-lib',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'idl',
        'proto_out_dir': 'include/soma/proto_bindings',
        'gen_bidl': 1,
      },
      'sources': [
        '<(proto_in_dir)/container_spec.proto',
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
        'spec_reader.cc',
        'sysfs_filter.cc',
        'soma.cc',
        'usb_device_filter.cc',
      ],
    },
    {
      'target_name': 'somad',
      'type': 'executable',
      'dependencies': [
        'libsomad',
      ],
      'sources': ['main.cc'],
    },
    {
      'target_name': 'soma_client',
      'type': 'executable',
      'dependencies': [
        'libsomad',
      ],
      'sources': ['soma_client.cc'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'soma_test',
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
      ],
    }],
  ],
}
