# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# export BOARD=wolf
# export AR=`portageq-$BOARD envvar AR`
# export CC=`portageq-$BOARD envvar CC`
# export CXX=`portageq-$BOARD envvar CXX`
# gyp -fmake --depth=. -DUSE_test=0 -Dplatform_root=$HOME/trunk/src/platform -Dsysroot=/build/$BOARD --include=../../platform/common-mk/common.gypi -Dpkg-config=pkg-config-$BOARD attestation.gyp
# make

{
  'variables': {
    'libbase_ver': 242728,
  },
  'target_defaults': {
    'dependencies': [
      '<(platform_root)/libchromeos/libchromeos-<(libbase_ver).gyp:*',
      '<(platform_root)/system_api/system_api.gyp:*',
    ],
    'variables': {
      'deps': [  # This is a list of pkg-config dependencies
        'libchrome-<(libbase_ver)',
      ]
    },
    'link_settings': {
      'libraries': [
        '-lprotobuf',
      ],
    },
    'cflags_cc': [ '-std=gnu++11' ],
  },
  'targets': [
    {
      'target_name': 'attestation',
      'type': 'executable',
      'sources': [
        'client/main.cc',
      ],
      'dependencies': [
        'attestation_proto'
      ]
    },
    {
      'target_name': 'attestationd',
      'type': 'executable',
      'sources': [
        'server/main.cc',
        'server/attestation_service.cc'
      ],
      'dependencies': [
        'attestation_proto'
      ],
    },
    {
      'target_name': 'attestation_proto',
      'type': 'static_library',
      'variables': {
        'proto_in_dir': 'common',
        'proto_out_dir': 'include/attestation/common',
      },
      'sources': [
        '<(proto_in_dir)/dbus_interface.proto'
      ],
      'includes': ['../common-mk/protoc.gypi'],
    },
  ],
}
