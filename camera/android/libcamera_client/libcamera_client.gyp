# gyplint: disable=GypLintVisibilityFlags
{
  'includes': [
    '../../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'cros-camera-android-headers',
        'libcamera_metadata',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_client',
      'type': 'shared_library',
      'cflags': [
        '-I<(src_root_path)/android/libcamera_client/include',
        # We don't want to modify the Android sources to add the visibility
        # attributes, so allow -fvisibility=default here.
        '-fvisibility=default',
      ],
      'sources': [
        'src/camera_metadata.cc',
      ],
    },
  ],
}
