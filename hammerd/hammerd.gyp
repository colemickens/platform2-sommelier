{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libbrillo-<(libbase_ver)',
        'libusb-1.0',
        'fmap',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libhammerd',
      'type': 'static_library',
      'sources': [
        'hammer_updater.cc',
        'update_fw.cc',
        'usb_utils.cc',
      ],
    },
    {
      'target_name': 'hammerd',
      'type': 'executable',
      'dependencies': ['libhammerd'],
      'sources': [
        'main.cc',
      ],
    },
  ],
}
