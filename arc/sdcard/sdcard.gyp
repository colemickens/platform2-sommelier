{
  'targets': [
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
            '-p', '/mnt d 700 0 0',
            '-p', '/mnt/runtime d 700 0 0',
            '-p', '/proc d 700 0 0',
            '-p', '/system d 700 0 0',
            '-p', '/system/bin d 700 0 0',
            '-p', '/system/bin/sdcard f 640 0 0 echo -n',
            '-p', '/Downloads d 700 0 0',
          ],
        },
      ],
    },
  ],
}
