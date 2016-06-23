# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcontainer_overrides',
      'type': 'shared_library',
      'sources': [
        'preload.c',
        'broker_client.c',
        'broker_client.h',
      ],
      'libraries': [
        '-ldl',
      ],
    },
  ],
}
