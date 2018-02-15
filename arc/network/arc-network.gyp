{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libndp',
        'libshill-client',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'include/arc/network',
      },
      'sources': ['<(proto_in_dir)/ipc.proto'],
      'includes': ['../../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'arc-networkd',
      'type': 'executable',
      'dependencies': ['protos'],
      'sources': [
        'arc_ip_config.cc',
        'dns/big_endian.cc',
        'dns/dns_response.cc',
        'dns/io_buffer.cc',
        'helper_process.cc',
        'ip_helper.cc',
        'main.cc',
        'manager.cc',
        'multicast_forwarder.cc',
        'multicast_socket.cc',
        'ndp_handler.cc',
        'neighbor_finder.cc',
        'router_finder.cc',
        'shill_client.cc',
      ],
    },
  ],
}
