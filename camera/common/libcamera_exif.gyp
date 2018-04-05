{
  'includes': [
    '../build/cros-camera-common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'deps': [
        'libexif',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'libcamera_exif',
      'type': 'shared_library',
      'sources': [
        'exif_utils.cc',
      ],
    },
  ],
}
