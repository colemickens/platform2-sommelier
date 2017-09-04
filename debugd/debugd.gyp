{
  'target_defaults': {
    'variables': {
      'deps': [
        'dbus-1',
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libminijail',
        'libpcrecpp',
      ],
    },
    'defines': [
      'USE_CELLULAR=<(USE_cellular)',
      'USE_WIMAX=<(USE_wimax)',
    ],
  },
  'targets': [
    # External library protocol buffers needed to call D-Bus functions.
    {
      'target_name': 'external-proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': '<(sysroot)/usr/include/chromeos/dbus/cryptohome',
        'proto_out_dir': 'include',
      },
      'sources': [
        '<(proto_in_dir)/key.proto',
        '<(proto_in_dir)/rpc.proto',
      ],
      'includes': [
        '../common-mk/protoc.gypi',
      ],
    },
    {
      'target_name': 'debugd-adaptors',
      'type': 'none',
      'variables': {
        'dbus_service_config': 'dbus_bindings/dbus-service-config.json',
        'dbus_adaptors_out_dir': 'include/debugd/dbus_adaptors',
      },
      'sources': [
        'dbus_bindings/org.chromium.debugd.xml',
      ],
      'includes': ['../common-mk/generate-dbus-adaptors.gypi'],
    },
    {
      'target_name': 'debugd-proxies',
      'type': 'none',
      'variables': {
        'exported_deps': [
          'libshill-client',
        ],
        'deps': ['>@(exported_deps)'],
      },
      'all_dependent_settings': {
        'variables': {
          'deps': [
            '<@(exported_deps)',
          ],
        },
      },
    },
    {
      'target_name': 'libdebugd',
      'type': 'static_library',
      'dependencies': [
        'debugd-proxies',
        'debugd-adaptors',
        'external-proto',
      ],
      'sources': [
        'src/anonymizer_tool.cc',
        'src/battery_tool.cc',
        'src/constants.cc',
        'src/container_tool.cc',
        'src/crash_sender_tool.cc',
        'src/cups_tool.cc',
        'src/debug_logs_tool.cc',
        'src/debug_mode_tool.cc',
        'src/debugd_dbus_adaptor.cc',
        'src/dev_features_tool.cc',
        'src/dev_mode_no_owner_restriction.cc',
        'src/example_tool.cc',
        'src/icmp_tool.cc',
        'src/log_tool.cc',
        'src/memory_tool.cc',
        'src/modem_status_tool.cc',
        'src/netif_tool.cc',
        'src/network_status_tool.cc',
        'src/oom_adj_tool.cc',
        'src/packet_capture_tool.cc',
        'src/perf_tool.cc',
        'src/ping_tool.cc',
        'src/process_with_id.cc',
        'src/process_with_output.cc',
        'src/route_tool.cc',
        'src/sandboxed_process.cc',
        'src/session_manager_proxy.cc',
        'src/storage_tool.cc',
        'src/subprocess_tool.cc',
        'src/swap_tool.cc',
        'src/sysrq_tool.cc',
        'src/systrace_tool.cc',
        'src/tracepath_tool.cc',
        'src/u2f_tool.cc',
        'src/variant_utils.cc',
        'src/wifi_debug_tool.cc',
        'src/wifi_power_tool.cc',
        'src/wimax_status_tool.cc',
      ],
    },
    {
      'target_name': 'debugd_dbus_utils',
      'type': 'static_library',
      'sources': [
        'src/helpers/shill_proxy.cc',
        'src/helpers/system_service_proxy.cc',
      ],
    },
    {
      'target_name': 'debugd',
      'type': 'executable',
      'dependencies': [
        'libdebugd',
      ],
      'variables': {
        'deps': [
          'protobuf',
        ],
      },
      'sources': [
        'src/main.cc',
      ],
    },
    {
      'target_name': 'capture_packets',
      'type': 'executable',
      'libraries': [
        '-lpcap',
      ],
      'sources': [
        'src/helpers/capture_packets.cc',
      ],
    },
    {
      'target_name': 'dev_features_chrome_remote_debugging',
      'type': 'executable',
      'dependencies': [
        'libdebugd',
      ],
      'sources': [
        'src/helpers/dev_features_chrome_remote_debugging.cc',
      ],
    },
    {
      'target_name': 'dev_features_password',
      'type': 'executable',
      'dependencies': [
        'libdebugd',
      ],
      'sources': [
        'src/helpers/dev_features_password.cc',
        'src/helpers/dev_features_password_utils.cc',
      ],
    },
    {
      'target_name': 'dev_features_rootfs_verification',
      'type': 'executable',
      'dependencies': [
        'libdebugd',
      ],
      'link_settings': {
        'libraries': [
          '-lrootdev',
        ],
      },
      'sources': [
        'src/helpers/dev_features_rootfs_verification.cc',
      ],
    },
    {
      'target_name': 'dev_features_ssh',
      'type': 'executable',
      'dependencies': [
        'libdebugd',
      ],
      'sources': [
        'src/helpers/dev_features_ssh.cc',
      ],
    },
    {
      'target_name': 'dev_features_usb_boot',
      'type': 'executable',
      'dependencies': [
        'libdebugd',
      ],
      'link_settings': {
        'libraries': [
          '-lvboot_host',
        ],
      },
      'sources': [
        'src/helpers/dev_features_usb_boot.cc',
      ],
    },
    {
      'target_name': 'icmp',
      'type': 'executable',
      'sources': [
        'src/helpers/icmp.cc',
      ],
    },
    {
      'target_name': 'netif',
      'type': 'executable',
      'dependencies': [
        'debugd_dbus_utils',
      ],
      'sources': [
        'src/helpers/netif.cc',
      ],
    },
    {
      'target_name': 'network_status',
      'type': 'executable',
      'dependencies': [
        'debugd_dbus_utils',
      ],
      'sources': [
        'src/helpers/network_status.cc',
      ],
    },
    {
      'target_name': 'generate_logs',
      'type': 'executable',
      'sources': ['tools/generate_logs.cc'],
    },
  ],
  'conditions': [
    ['USE_cellular == 1', {
      'targets': [
        {
          'target_name': 'modem_status',
          'type': 'executable',
          'dependencies': [
            'debugd_dbus_utils',
          ],
          'sources': [
            'src/helpers/modem_status.cc',
          ],
        },
      ],
    }],
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'debugd_testrunner',
          'type': 'executable',
          'dependencies': [
            'libdebugd',
            'debugd_dbus_utils',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'protobuf',
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'includes': ['../common-mk/common_test.gypi'],
          'libraries': ['-lm',],
          'sources': [
            'src/anonymizer_tool_test.cc',
            'src/dev_mode_no_owner_restriction_test.cc',
            'src/helpers/dev_features_password_utils.cc',
            'src/helpers/dev_features_password_utils_test.cc',
            'src/log_tool_test.cc',
            'src/modem_status_tool_test.cc',
            'src/process_with_id_test.cc',
            'src/sandboxed_process_test.cc',
            'src/subprocess_tool_test.cc',
          ],
        },
      ],
    }],
    ['USE_wimax == 1', {
      'targets': [
        {
          'target_name': 'wimax_status',
          'type': 'executable',
          'dependencies': [
            'debugd_dbus_utils',
          ],
          'sources': [
            'src/helpers/wimax_status.cc',
          ],
        },
      ],
    }],
  ],
}
