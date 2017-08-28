{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'net_poll_tool',
      'type': 'executable',
      'sources': ['net_poll_tool.cc'],
    },
    {
      'target_name': 'static_node_tool',
      'type': 'executable',
      'sources': ['static_node_tool.cc'],
    },
  ],
}
