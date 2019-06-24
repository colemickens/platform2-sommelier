# Caution!: GYP to GN migration is happening. If you update this file, please
# also update //camera/build/BUILD.gn accordingly.

{
  'variables': {
    'src_root_path': '<(platform2_root)/camera',
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
