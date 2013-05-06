{
  'target_defaults': {
    'dependencies': [
      '../libchromeos/libchromeos-<(libbase_ver).gyp:libchromeos-<(libbase_ver)',
      '../metrics/metrics.gyp:libmetrics',
    ],
    'libraries': [
      '-lgflags',
      '-lminijail',
      '-lrootdev',
    ],
    'variables': {
      'deps': [
        'blkid',
        'dbus-c++-1',
        'glib-2.0',
        'gobject-2.0',
        'gthread-2.0',
        'libchrome-<(libbase_ver)',
        'libparted',
        'libudev',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libdisks-adaptors',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_out_dir': 'include/cros-disks',
      },
      'sources': [
        'cros-disks-server.xml',
      ],
      'includes': ['../common-mk/xml2cpp.gypi'],
    },
    {
      'target_name': 'libdisks',
      'type': 'static_library',
      'dependencies': ['libdisks-adaptors'],
      'sources': [
        'archive-manager.cc',
        'cros-disks-server-impl.cc',
        'daemon.cc',
        'device-ejector.cc',
        'device-event-moderator.cc',
        'device-event-queue.cc',
        'device-event.cc',
        'disk-manager.cc',
        'disk.cc',
        'exfat-mounter.cc',
        'external-mounter.cc',
        'file-reader.cc',
        'filesystem.cc',
        'format-manager.cc',
        'fuse-mounter.cc',
        'glib-process.cc',
        'metrics.cc',
        'mount-info.cc',
        'mount-manager.cc',
        'mount-options.cc',
        'mounter.cc',
        'ntfs-mounter.cc',
        'platform.cc',
        'process.cc',
        'sandboxed-process.cc',
        'session-manager-proxy.cc',
        'system-mounter.cc',
        'udev-device.cc',
        'usb-device-info.cc',
      ],
    },
    {
      'target_name': 'disks',
      'type': 'executable',
      'dependencies': ['libdisks'],
      'sources': [
        'main.cc',
      ]
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'disks_testrunner',
          'type': 'executable',
          'dependencies': ['libdisks'],
          'includes': ['../common-mk/common_test.gypi'],
          'sources': [
            'archive-manager_unittest.cc',
            'device-event-moderator_unittest.cc',
            'device-event-queue_unittest.cc',
            'disk-manager_unittest.cc',
            'external-mounter_unittest.cc',
            'disk_unittest.cc',
            'disks_testrunner.cc',
            'file-reader_unittest.cc',
            'format-manager_unittest.cc',
            'glib-process_unittest.cc',
            'metrics_unittest.cc',
            'mount-info_unittest.cc',
            'mount-manager_unittest.cc',
            'mount-options_unittest.cc',
            'mounter_unittest.cc',
            'platform_unittest.cc',
            'process_unittest.cc',
            'system-mounter_unittest.cc',
            'udev-device_unittest.cc',
            'usb-device-info_unittest.cc',
          ]
        },
      ],
    }],
  ],
}
