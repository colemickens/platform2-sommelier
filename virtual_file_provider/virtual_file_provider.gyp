# TODO: Rename these files to pass this check.
# gyplint: disable=GypLintSourceFileNames

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
      'dependencies': [
        'libvirtual-file-provider',
      ],
      'sources': [
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
      'target_name': 'libvirtual-file-provider',
      'type': 'static_library',
      'defines': [
        'FUSE_USE_VERSION=26',
      ],
      'sources': [
        'fuse_main.cc',
        'operation_throttle.cc',
        'service.cc',
        'size_map.cc',
        'util.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'virtual-file-provider_testrunner',
          'type': 'executable',
          'dependencies': [
            'libvirtual-file-provider',
            '../common-mk/testrunner.gyp:testrunner',
          ],
          'sources': [
            'operation_throttle_unittest.cc',
          ],
        },
      ],
    }],
  ],
}
