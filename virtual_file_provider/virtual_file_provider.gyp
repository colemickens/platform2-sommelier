{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        # system_api depends on protobuf (or protobuf-lite). It must appear
        # before protobuf here or the linker flags won't be in the right
        # order.
        'system_api',
        'protobuf-lite',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'virtual-file-provider',
      'type': 'executable',
      'deps': [
        'libcap',
      ],
      'defines': [
        'FUSE_USE_VERSION=26',
      ],
      'sources': [
        'fuse_main.cc',
        'service.cc',
        'util.cc',
        'virtual_file_provider.cc',
      ],
      'variables': {
        'deps': [
          'libcap',
          'fuse',
        ],
      },
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
            '-p', '/bin d 700 0 0',
            '-p', '/etc d 700 0 0',
            '-p', '/lib d 700 0 0',
            '-p', '/proc d 700 0 0',
            '-p', '/sbin d 700 0 0',
            '-p', '/usr d 700 0 0',
            '-p', '/dev d 755 0 0',
            '-p', '/dev/null c 666 0 0 1 3',
            '-p', '/dev/fuse c 666 0 0 10 229',
            '-p', '/dev/urandom c 666 0 0 1 9',
            '-p', '/mnt d 755 0 0',
            '-p', '/run d 755 0 0',
            '-p', '/run/dbus d 755 0 0',
          ],
        },
      ],
    },
  ],
}
