# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libdrm',
        'libpng',
        'gbm',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'screenshot',
      'type': 'executable',
      'sources': ['screenshot.cc'],
    },
  ],
}
