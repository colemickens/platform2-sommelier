{
  'target_defaults': {
    'defines': [
      'FUSE_USE_VERSION=26',
    ],
    'variables': {
      'deps': [
        'fuse',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libcap',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libmount_obb',
      'type': 'static_library',
      'sources': [
        'util.cc',
        'volume.cc',
      ],
    },
    {
      'target_name': 'arc-obb-mounter',
      'type': 'executable',
      'sources': [
        'arc_obb_mounter.cc',
        'mount.cc',
        'service.cc',
      ],
    },
    {
      'target_name': 'mount-obb',
      'type': 'executable',
      'sources': ['mount_obb.cc'],
      'dependencies': ['libmount_obb'],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'arc-obb-mounter_testrunner',
          'type': 'executable',
          'includes': ['../../common-mk/common_test.gypi'],
          'defines': ['UNIT_TEST'],
          'dependencies': [
            'libmount_obb',
            '../../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': ['util_unittest.cc'],
        },
      ],
    }],
  ],
}
