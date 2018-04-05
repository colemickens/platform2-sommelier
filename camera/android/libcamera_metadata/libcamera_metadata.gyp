{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'cros-camera-android-headers',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_metadata',
      'type': 'shared_library',
      'cflags': [
        '-I<(src_root_path)/android/libcamera_metadata/include',
      ],
      'sources': [
        'src/camera_metadata.c',
      ],
    },
  ],
}
