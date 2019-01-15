{
  'variables': {
    # platform/arc-camera
    'src_root_path': '<(platform2_root)/../platform/arc-camera',
  },
  'target_defaults': {
    'include_dirs': [
      '<(src_root_path)',
      '<(src_root_path)/include',
    ],
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
      ],
    },
  },
}
