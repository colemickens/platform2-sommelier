{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'common-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'include',
        'exported_deps': [
          'protobuf',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        '<(proto_in_dir)/common.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    {
      'target_name': 'container-rpcs',
      'type': 'static_library',
      'dependencies': [
        'common-protos',
      ],
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'include',
        'gen_grpc': 1,
        'exported_deps': [
          'grpc++',
          'protobuf',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        '<(proto_in_dir)/container_guest.proto',
        '<(proto_in_dir)/container_host.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    {
      'target_name': 'vm-rpcs',
      'type': 'static_library',
      'dependencies': [
        'common-protos',
      ],
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'include',
        'gen_grpc': 1,
        'exported_deps': [
          'grpc++',
          'protobuf',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        '<(proto_in_dir)/vm_guest.proto',
        '<(proto_in_dir)/vm_host.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    {
      'target_name': 'vsh-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'include',
        'exported_deps': [
          'protobuf',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
      'sources': [
        '<(proto_in_dir)/vsh.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    {
      'target_name': 'libvsh',
      'type': 'static_library',
      'sources': [
        'vsh/utils.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_kvm_host == 1', {
      'includes': ['./host.gypi'],
    }],
    ['USE_kvm_host == 0', {
      'includes': ['./guest.gypi'],
    }],
  ],
}
