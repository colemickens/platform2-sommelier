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
    {
      'target_name': 'rootfs.squashfs',
      'type': 'none',
      'actions': [
        {
          'action_name': 'mkdir_squashfs',
          'inputs': [],
          'outputs': [
            '<(INTERMEDIATE_DIR)/squashfs',
          ],
          'action': [
            'mkdir', '-p', '<(INTERMEDIATE_DIR)/squashfs',
          ],
        },
        {
          'action_name': 'create_squashfs',
          'inputs': [
            '<(INTERMEDIATE_DIR)/squashfs',
          ],
          'outputs':[
            'rootfs.squashfs',
          ],
          'action': [
            'mksquashfs',
            '<(INTERMEDIATE_DIR)/squashfs',
            '<(PRODUCT_DIR)/rootfs.squashfs',
            '-no-progress',
            '-info',
            '-all-root',
            '-noappend',
            '-comp', 'lzo',
            '-b', '4K',

            '-p', '/bin        d 700 0 0',
            '-p', '/etc        d 700 0 0',
            '-p', '/lib        d 700 0 0',
            '-p', '/proc       d 700 0 0',
            '-p', '/sbin       d 700 0 0',
            '-p', '/usr        d 700 0 0',

            '-p', '/dev        d 755 0 0',
            '-p', '/dev/null   c 666 0 0 1 3',
            '-p', '/dev/fuse   c 666 0 0 10 229',

            '-p', '/mnt        d 755 0 0',
            '-p', '/mnt/source d 700 0 0',
            '-p', '/mnt/dest   d 700 0 0',
          ],
        },
      ],
    },
  ],
}
