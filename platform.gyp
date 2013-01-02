{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/libchromeos/libchromeos-<(libbase_ver).gyp:*',
        '<(DEPTH)/metrics/metrics.gyp:*',
        '<(DEPTH)/system_api/system_api.gyp:*',
      ],
      'conditions': [
        ['USE_wimax == 1', {
          'dependencies': [
            '<(DEPTH)/wimax_manager/wimax_manager.gyp:*',
          ],
        }],
        ['USE_cros_host == 0', {
          'dependencies': [
            #'<(DEPTH)/bootcache/bootcache.gyp:*',
            #'<(DEPTH)/bootstat/bootstat.gyp:*',
            #'<(DEPTH)/crash-reporter/crash-reporter.gyp:*',
            '<(DEPTH)/cros-disks/cros-disks.gyp:*',
            #'<(DEPTH)/cros_boot_mode/cros_boot_mode.gyp:*',
            #'<(DEPTH)/debugd/debugd.gyp:*',
            #'<(DEPTH)/installer/installer.gyp:*',
            #'<(DEPTH)/libevdev/libevdev.gyp:*',
            #'<(DEPTH)/memento_softwareupdate/memento_softwareupdate.gyp:*',
            ##'<(DEPTH)/microbenchmark/microbenchmark.gyp:*',
            ##'<(DEPTH)/monitor_reconfig/monitor_reconfig.gyp:*',
            #'<(DEPTH)/mtplot/mtplot.gyp:*',
            #'<(DEPTH)/rollog/rollog.gyp:*',
            #'<(DEPTH)/theme/theme.gyp:*',
            #'<(DEPTH)/vpd/vpd.gyp:*',
            #'<(DEPTH)/vpn-manager/vpn-manager.gyp:*',
          ],
        }],
      ],
    }
  ],
}
