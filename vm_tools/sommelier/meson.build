project(
	'sommelier',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.48.0',
	default_options: [
		'c_std=c11',
#		'warning_level=2',
#		'werror=true',
	],
)

add_project_arguments(
	[
		'-Wno-unused-parameter',
		'-Wno-missing-field-initializers',

		'-D_GNU_SOURCE',
		'-DWL_HIDE_DEPRECATED',
		'-DXWAYLAND_PATH="Xwayland"',
		'-DXWAYLAND_GL_DRIVER_PATH=""',
		'-DXWAYLAND_SHM_DRIVER="noop"',
		'-DSHM_DRIVER="noop"',
		'-DPEER_CMD_PREFIX=""',
		'-DVIRTWL_DEVICE=NULL',
		'-DFRAME_COLOR="#f2f2f2"',
		'-DDARK_FRAME_COLOR="#323639"',
	],
	language: 'c',
)

cc = meson.get_compiler('c')

wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.7')
pixman         = dependency('pixman-1')
gbm            = dependency('gbm')
xcb            = dependency('xcb')
xcb_composite  = dependency('xcb-composite')
xcb_xfixes     = dependency('xcb-xfixes')
xkbcommon      = dependency('xkbcommon')
drm            = dependency('libdrm')
math           = cc.find_library('m')

subdir('protocol')

sommelier_files = [
    'sommelier-compositor.c',
    'sommelier-data-device-manager.c',
    'sommelier-display.c',
    'sommelier-drm.c',
    'sommelier-gtk-shell.c',
    'sommelier-output.c',
    'sommelier-pointer-constraints.c',
    'sommelier-relative-pointer-manager.c',
    'sommelier-seat.c',
    'sommelier-shell.c',
    'sommelier-shm.c',
    'sommelier-subcompositor.c',
    'sommelier-text-input.c',
    'sommelier-viewporter.c',
    'sommelier-xdg-shell.c',
    'sommelier.c',
]

executable(
	'sommelier',
	sommelier_files,
	dependencies: [
		gbm,
		pixman,
		wayland_client,
		wayland_server,
		wayland_protos,
		xcb,
		xcb_composite,
		xcb_xfixes,
		xkbcommon,
		drm,
		math,
		sommelier_protos,
	],
	install: true,
)

