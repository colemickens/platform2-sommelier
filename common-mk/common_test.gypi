# This file is included for every single test target.
# You can add anything in here that's valid in a target dictionary.

{
  'link_settings': {
    'libraries+': [
      # Don't worry about overlinking, ld.gold's --as-needed will
      # deal with that.
      '>!@(gtest-config --libs)',
      '>!@(gmock-config --libs)',
    ],
  },
}
