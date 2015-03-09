{
  'target_defaults': {
    'defines': [
      '__STDC_FORMAT_MACROS',
    ],
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'protobuf-lite',
      ],
    },
  },
  'targets': [    {
      'target_name': 'soma-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'idl',
        'proto_out_dir': 'include/soma/proto_bindings',
      },
      'sources': [
        '<(proto_in_dir)/container_spec.proto',
        '<(proto_in_dir)/soma.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libsoma',
      'type': 'static_library',
      'dependencies': [
        'soma-protos',
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
            'libsoma',
            'soma-protos',
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
