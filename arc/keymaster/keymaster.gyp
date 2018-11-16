# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'mojo_output_dir': '<(SHARED_INTERMEDIATE_DIR)/include',
  },
  'targets': [
    # Binary
    {
      'target_name': 'arc-keymasterd',
      'type': 'executable',
      'sources': [
        '<(mojo_output_dir)/mojo/keymaster.mojom.cc',
        'daemon.cc',
        'main.cc',
      ],
      'dependencies': [
        'keymaster_mojo_bindings',
      ],
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
    },

    # Mojo bindings
    {
      'target_name': 'keymaster_mojo_bindings',
      'type': 'static_library',
      'sources': [
        'mojo/keymaster.mojom',
      ],
      'includes': ['../../common-mk/mojom_bindings_generator.gypi'],
    },
  ],
}
