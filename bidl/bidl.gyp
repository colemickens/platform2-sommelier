# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'protoc-gen-bidl',
      'type': 'executable',
      'variables': {
        'deps': [
          'protobuf',
        ],
      },
      'libraries': [
        '-lprotoc',
        '-lprotobuf',
      ],
      'sources': [
        'bidl.cc',
        'bidl_code_generator.cc'
      ],
    },
  ],
}
