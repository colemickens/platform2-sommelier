{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libchromeos-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libprotobinder',
      'type': 'shared_library',
      'sources': [
        'binder_daemon.cc',
        'binder_manager.cc',
        'parcel.cc',
        'binder_proxy.cc',
        'binder_host.cc',
        'ibinder.cc',
        'iservice_manager.cc',
        'binder_proxy_interface_base.cc',
      ],
    },
    {
      'target_name': 'ping-daemon',
      'type': 'executable',
      'sources': [
        'samples/ping_daemon.cc',
      ],
      'dependencies': [
        'libprotobinder',
      ],
    },
    {
      'target_name': 'ping-client',
      'type': 'executable',
      'sources': [
        'samples/ping_client.cc',
      ],
      'dependencies': [
        'libprotobinder',
      ],
    },
  ],
}
