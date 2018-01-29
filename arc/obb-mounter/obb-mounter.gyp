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
    {
      'target_name': 'rootfs.squashfs',
      'type': 'none',
      'actions': [
        {
          'action_name': 'mkdir_squashfs_source_dir',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/squashfs_source_dir',
          ],
          'action': [
            'mkdir', '-p', '<(INTERMEDIATE_DIR)/squashfs_source_dir',
          ],
        },
        {
          'action_name': 'generate_squashfs',
          'inputs': [
            '<(INTERMEDIATE_DIR)/squashfs_source_dir',
          ],
          'outputs': [
            'rootfs.squashfs',
          ],
          'action': [
            'mksquashfs',
            '<(INTERMEDIATE_DIR)/squashfs_source_dir',
            '<(PRODUCT_DIR)/rootfs.squashfs',
            '-no-progress',
            '-info',
            '-all-root',
            '-noappend',
            '-comp', 'lzo',
            '-b', '4K',
            '-p', '/data d 700 0 0',
            '-p', '/dev d 700 0 0',
            '-p', '/dev/fuse c 666 root root 10 229',
            '-p', '/lib d 700 0 0',
            '-p', '/lib64 d 700 0 0',
            '-p', '/proc d 700 0 0',
            '-p', '/run d 700 0 0',
            '-p', '/run/dbus d 700 0 0',
            '-p', '/usr d 700 0 0',
            '-p', '/var d 700 0 0',
            '-p', '/var/run d 700 0 0',
            '-p', '/var/run/arc d 700 0 0',
            '-p', '/var/run/arc/obb d 700 0 0',
          ],
        },
      ],
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
