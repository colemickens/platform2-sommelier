{
  'target_defaults': {
    'variables': {
      'deps': [
        'libchrome-<(libbase_ver)',
        'libpng',
        'x11',
        'libgflags',
      ],
    },
    'link_settings': {
      'libraries': [
        '-lrt',
      ],
    },
    'conditions': [
      [
        'USE_opengles == 1', {
          # GRAPHICS_BACKEND: OPENGLES
          'variables': {
            'deps': [
              'glesv2',
              'egl',
            ],
          },
          'defines': ['USE_OPENGLES'],
        },
        {
          # GRAPHICS_BACKEND: OPENGL
          'variables': {
            'deps': [
              'gl',
            ],
          },
          'defines': ['USE_OPENGL'],
        }
      ]
    ],
    'include_dirs': ['.'],
  },
  'targets': [
    {
      'target_name': 'glbench_common',
      'type': 'static_library',
      'sources': [
        'xlib_window.cc',
        'utils.cc',
      ],
      'conditions': [
        [
          'USE_opengles == 1',
              {'sources': ['egl_stuff.cc']},
              {'sources': ['glx_stuff.cc']}
        ]
      ],
    },
    {
      'target_name': 'glbench',
      'type': 'executable',
      'dependencies': [
        'glbench_common',
      ],
      'sources': [
        'attributefetchtest.cc',
        'cleartest.cc',
        'contexttest.cc',
        'fillratetest.cc',
        'glinterfacetest.cc',
        'main.cc',
        'md5.cc',
        'png_helper.cc',
        'readpixeltest.cc',
        'swaptest.cc',
        'testbase.cc',
        'texturereusetest.cc',
        'texturetest.cc',
        'textureupdatetest.cc',
        'textureuploadtest.cc',
        'trianglesetuptest.cc',
        'varyingsandddxytest.cc',
        'windowmanagercompositingtest.cc',
        'yuvtest.cc',
      ],
    },
    {
      'target_name': 'synccontroltest',
      'type': 'executable',
      'dependencies': [
        'glbench_common',
      ],
      'sources': [
        'synccontroltest.cc',
      ],
      'conditions': [
        [
          'USE_opengles == 1',
              {'sources': ['synccontroltest_egl.cc']},
              {'sources': ['synccontroltest_glx.cc']}
        ]
      ],
    },
    {
      'target_name': 'teartest',
      'type': 'executable',
      'dependencies': [
        'glbench_common',
      ],
      'sources': [
        'teartest.cc',
      ],
      'conditions': [
        [
          'USE_opengles == 1',
              {'sources': ['teartest_egl.cc']},
              {'sources': ['teartest_glx.cc']}
        ]
      ],
    },
    {
      'target_name': 'windowmanagertest',
      'type': 'executable',
      'dependencies': [
        'glbench_common',
      ],
      'sources': [
        'windowmanagertest.cc',
      ],
    },
  ],
}
