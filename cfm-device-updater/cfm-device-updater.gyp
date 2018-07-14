# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
        'libusb-1.0',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'bizlink-updater',
      'type': 'executable',
      'sources': [
        'bizlink-updater/src/dp_aux_ctrl.cc',
        'bizlink-updater/src/main.cc',
        'bizlink-updater/src/mcdp_chip_ctrl.cc',
      ],
    },
  ],
}
