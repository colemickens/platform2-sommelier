# Caution!: GYP to GN migration is happening. If you update this file, please
# update "test" config in common-mk/BUILD.gn too accordingly.

# This file is included for every single test target.
# You can add anything in here that's valid in a target dictionary.

{
  'link_settings': {
    'libraries+': [
      # Don't worry about overlinking, ld.gold's --as-needed will
      # deal with that.
      # gtest-config and gmock-config are not installed by gtest-1.8.1.
      '-lgmock', '-lgtest', '-pthread', '-lpthread',
    ],
  },
}
