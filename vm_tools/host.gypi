{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'maitred_client',
      'type': 'executable',
      'dependencies': ['vm-rpcs'],
      'sources': [
        'maitred/client.cc',
      ],
    },
    {
      'target_name': 'vm_launcher',
      'type': 'executable',
      'sources': [
        'launcher/mac_address.cc',
        'launcher/nfs_launcher.cc',
        'launcher/pooled_resource.cc',
        'launcher/subnet.cc',
        'launcher/vm_launcher.cc',
      ],
    },
  ],
}
