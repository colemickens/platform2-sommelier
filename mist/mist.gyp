# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libmetrics-<(libbase_ver)',
        'libudev',
        'libusb-1.0',
        'protobuf',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'mist-protos',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'proto',
        'proto_out_dir': 'include/mist/proto_bindings',
      },
      'sources': [
        '<(proto_in_dir)/config.proto',
        '<(proto_in_dir)/usb_modem_info.proto',
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
    {
      'target_name': 'libmist',
      'type': 'static_library',
      'dependencies': ['mist-protos'],
      'sources': [
        'config_loader.cc',
        'context.cc',
        'event_dispatcher.cc',
        'metrics.cc',
        'mist.cc',
        'udev.cc',
        'udev_device.cc',
        'udev_enumerate.cc',
        'udev_list_entry.cc',
        'udev_monitor.cc',
        'usb_bulk_transfer.cc',
        'usb_config_descriptor.cc',
        'usb_constants.cc',
        'usb_device.cc',
        'usb_device_descriptor.cc',
        'usb_device_event_notifier.cc',
        'usb_endpoint_descriptor.cc',
        'usb_error.cc',
        'usb_interface.cc',
        'usb_interface_descriptor.cc',
        'usb_manager.cc',
        'usb_modem_one_shot_switcher.cc',
        'usb_modem_switch_context.cc',
        'usb_modem_switch_operation.cc',
        'usb_modem_switcher.cc',
        'usb_transfer.cc',
      ],
    },
    {
      'target_name': 'mist',
      'type': 'executable',
      'dependencies': ['libmist'],
      'sources': [
        'main.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'mist_testrunner',
          'type': 'executable',
          'dependencies': [
            'libmist',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'config_loader_unittest.cc',
            'event_dispatcher_unittest.cc',
            'mist.cc',
            'mock_context.cc',
            'usb_config_descriptor_unittest.cc',
            'usb_constants_unittest.cc',
            'usb_device_descriptor_unittest.cc',
            'usb_device_event_notifier_unittest.cc',
            'usb_endpoint_descriptor_unittest.cc',
            'usb_error_unittest.cc',
            'usb_interface_descriptor_unittest.cc',
            'usb_modem_switch_context_unittest.cc',
            'usb_transfer_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
