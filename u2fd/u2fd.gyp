{
  'target_defaults' : {
    'variables' : {
      'deps' : [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libpower_manager-client',
        'libtrunks',
        'system_api',
      ],
    },
  },
  'targets' : [
    {
      'target_name' : 'u2fd',
      'type' : 'executable',
      'dependencies': [
        '../common-mk/external_dependencies.gyp:policy-protos',
      ],
      'link_settings' : {
        'libraries' : [
          '-lpolicy-<(libbase_ver)',
        ],
      },
      'sources' : [
        'main.cc',
        'tpm_vendor_cmd.cc',
        'u2fhid.cc',
        'uhid_device.cc',
      ],
    },
  ],
}
