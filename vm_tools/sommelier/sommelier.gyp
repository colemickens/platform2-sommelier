{
  'conditions': [
    ['USE_amd64 == 1', {
      'variables': {
        'peer_cmd_prefix%': '"/opt/google/cros-containers/lib/ld-linux-x86-64.so.2 --library-path /opt/google/cros-containers/lib --inhibit-rpath \\"\\""',
      },
    }],
    ['USE_arm == 1', {
      'variables': {
        'peer_cmd_prefix%': '"/opt/google/cros-containers/lib/ld-linux-armhf.so.3 --library-path /opt/google/cros-containers/lib --inhibit-rpath \\"\\""',
      },
    }],
  ],
  'variables': {
    # Set this to the Xwayland path.
    'xwayland_path%': '"/opt/google/cros-containers/bin/Xwayland"',

    # Set this to the GL driver path to use for Xwayland.
    'xwayland_gl_driver_path%': '"/opt/google/cros-containers/lib"',

    # Set this to the shm driver to use for Xwayland.
    'xwayland_shm_driver%': '"virtwl"',

    # Set this to the shm driver to use for wayland clients.
    'shm_driver%': '"virtwl-dmabuf"',

    # Set this to the virtwl device.
    'virtwl_device%': '"/dev/wl0"',

    # Set this to the frame color to use for Xwayland clients.
    'frame_color%': '"#f2f2f2"',

    # Set this to the dark frame color to use for Xwayland clients.
    'dark_frame_color%': '"#323639"',
  },
  'targets': [
    {
      'target_name': 'sommelier-protocol',
      'type': 'static_library',
      'variables': {
        'wayland_in_dir': 'protocol',
        'wayland_out_dir': '.',
      },
      'sources': [
        'protocol/aura-shell.xml',
        'protocol/drm.xml',
        'protocol/gtk-shell.xml',
        'protocol/keyboard-extension-unstable-v1.xml',
        'protocol/linux-dmabuf-unstable-v1.xml',
        'protocol/text-input-unstable-v1.xml',
        'protocol/viewporter.xml',
        'protocol/xdg-shell-unstable-v6.xml',
      ],
      'includes': ['wayland-protocol.gypi'],
    },
    {
      'target_name': 'sommelier',
      'type': 'executable',
      'variables': {
        'exported_deps': [
          'gbm',
          'libdrm',
          'pixman-1',
          'wayland-client',
          'wayland-server',
          'xcb',
          'xcb-composite',
          'xcb-xfixes',
          'xkbcommon',
        ],
        'deps': ['<@(exported_deps)'],
      },
      'link_settings': {
        'libraries': [
          '-lm',
        ],
      },
      'dependencies': [
        'sommelier-protocol',
      ],
      'sources': [
        'sommelier-compositor.c',
        'sommelier-data-device-manager.c',
        'sommelier-display.c',
        'sommelier-drm.c',
        'sommelier-gtk-shell.c',
        'sommelier-output.c',
        'sommelier-seat.c',
        'sommelier-shell.c',
        'sommelier-shm.c',
        'sommelier-subcompositor.c',
        'sommelier-text-input.c',
        'sommelier-viewporter.c',
        'sommelier-xdg-shell.c',
        'sommelier.c',
      ],
      'defines': [
        '_GNU_SOURCE',
        'WL_HIDE_DEPRECATED',
        'XWAYLAND_PATH=<@(xwayland_path)',
        'XWAYLAND_GL_DRIVER_PATH=<@(xwayland_gl_driver_path)',
        'XWAYLAND_SHM_DRIVER=<@(xwayland_shm_driver)',
        'SHM_DRIVER=<@(shm_driver)',
        'VIRTWL_DEVICE=<@(virtwl_device)',
        'PEER_CMD_PREFIX=<@(peer_cmd_prefix)',
        'FRAME_COLOR=<@(frame_color)',
        'DARK_FRAME_COLOR=<@(dark_frame_color)',
      ],
    },
    {
      'target_name': 'wayland_demo',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
          'wayland-client',
        ],
      },
      'link_settings': {
        'libraries': [
          '-lwayland-client',
        ],
      },
      'sources': [
        'demos/wayland_demo.cc',
      ],
    },
    {
      'target_name': 'x11_demo',
      'type': 'executable',
      'variables': {
        'deps': [
          'libbrillo-<(libbase_ver)',
          'libchrome-<(libbase_ver)',
        ],
      },
      'link_settings': {
        'libraries': [
          '-lX11',
        ],
      },
      'sources': [
        'demos/x11_demo.cc',
      ],
    },
  ],
}
