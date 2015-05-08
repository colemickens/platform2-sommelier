{
  'targets': [
    {
      'target_name': 'python-soma-proto-lib',
      'type': 'none',
      'variables': {
        'exported_deps': [],
        'proto_in_dir': 'idl',
        'proto_out_dir': 'protos',
        'gen_python': 1,
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
        '<(proto_in_dir)/soma_sandbox_spec.proto',

      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
  ]
}

