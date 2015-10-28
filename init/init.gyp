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
      'target_name': 'static_node_tool',
      'type': 'executable',
      'sources': ['static_node_tool.cc'],
    },
  ],
}
