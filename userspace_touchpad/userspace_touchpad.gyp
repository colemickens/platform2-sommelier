{
  'targets': [
    {
      'target_name': 'userspace_touchpad',
      'type': 'executable',
      'sources': [
        'i2c-device.cc',
        'main.cc',
        'touch_emulator.cc',
        'uinputdevice.cc',
      ],
    },
  ],
}
