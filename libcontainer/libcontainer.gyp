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
    'defines': [
      'USE_device_mapper=<(USE_device_mapper)',
    ],
    'conditions': [
      ['USE_device_mapper == 1', {
        'variables': {
          'deps': [
            'devmapper',
          ],
        },
      }],
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
        'container_cgroup.cc',
        'libcontainer.cc',
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
            'libcontainer.cc',
            'libcontainer_unittest.cc',
          ],
        },
        {
          'target_name': 'container_cgroup_unittest',
          'type': 'executable',
          'sources': [
            'container_cgroup.cc',
            'container_cgroup_unittest.cc',
          ],
        },
      ]},
    ],
  ],
}
