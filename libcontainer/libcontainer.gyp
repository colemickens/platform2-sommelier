{
  'target_defaults': {
    'variables': {
      'deps': [
        'libminijail',
      ],
    },
    'include_dirs': [
      '.',
    ],
  },
  'targets': [
    {
      'target_name': 'libcontainer',
      'type': 'shared_library',
      'cflags': [
        '-fvisibility=default',
      ],
      'sources': [
        'libcontainer.c',
        'container_cgroup.c',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libcontainer_unittest',
          'type': 'executable',
          'sources': [
            'libcontainer.c',
            'libcontainer_unittest.c',
          ],
        },
        {
          'target_name': 'container_cgroup_unittest',
          'type': 'executable',
          'sources': [
            'container_cgroup.c',
            'container_cgroup_unittest.c',
          ],
        },
      ]},
    ],
  ],
}
