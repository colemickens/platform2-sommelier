{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets' : [
    {
      'target_name' : 'hermes',
      'type' : 'executable',
      'sources' : [
        'esim_uim_impl.cc',
        'lpd.cc',
        'main.cc',
        'smdp_fi_impl.cc',
      ],
    },
  ],
}
