{
  'targets': [
    {
      'target_name': 'mount-passthrough',
      'type': 'executable',
      'sources': [
        'mount-passthrough.cc',
      ],
      'variables': {
        'deps': [
          'fuse',
          'libcap',
        ],
      },
    },
  ],
}
