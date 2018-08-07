# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libminijail',
        'libchrome-<(libbase_ver)',
      ],
    },
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
      'sources': [
        'cgroup.cc',
        'config.cc',
        'container.cc',
        'libcontainer.cc',
        'libcontainer_util.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'libcontainer_test',
          'type': 'executable',
          'includes': ['../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'dependencies': [
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'ldflags': [
            '-Wl,-wrap=chmod',
            '-Wl,-wrap=chown',
            '-Wl,-wrap=getuid',
            '-Wl,-wrap=kill',
            '-Wl,-wrap=mkdir',
            '-Wl,-wrap=mkdtemp',
            '-Wl,-wrap=mount',
            '-Wl,-wrap=rmdir',
            '-Wl,-wrap=setns',
            '-Wl,-wrap=umount',
            '-Wl,-wrap=umount2',
            '-Wl,-wrap=unlink',
            '-Wl,-wrap=__xmknod',
            '-Wl,-wrap=__xstat',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'cgroup.cc',
            'cgroup_unittest.cc',
            'config.cc',
            'container.cc',
            'libcontainer.cc',
            'libcontainer_unittest.cc',
            'libcontainer_util.cc',
          ],
        },
      ]},
    ],
  ],
}
