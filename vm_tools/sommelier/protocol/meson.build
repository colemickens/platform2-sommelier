wayland_scanner_dep = dependency('wayland-scanner', required: false, native: true)
if wayland_scanner_dep.found()
	wayland_scanner = find_program(
		wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'),
		native: true,
	)
else
	wayland_scanner = find_program('wayland-scanner', native: true)
endif

protocols = [
    'aura-shell.xml',
    'drm.xml',
    'gtk-shell.xml',
    'keyboard-extension-unstable-v1.xml',
    'linux-dmabuf-unstable-v1.xml',
    'pointer-constraints-unstable-v1.xml',
    'relative-pointer-unstable-v1.xml',
    'text-input-unstable-v1.xml',
    'viewporter.xml',
    'xdg-shell.xml',
]

wl_protos_src = []
wl_protos_headers = []
foreach p : protocols
	xml = join_paths(p)
	wl_protos_src += custom_target(
		xml.underscorify() + '_server_c',
		input: xml,
		output: '@BASENAME@-protocol.c',
		command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
	)
	wl_protos_headers += custom_target(
		xml.underscorify() + '_server_h',
		input: xml,
		output: '@BASENAME@-server-protocol.h',
		command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
	)

	wl_protos_headers += custom_target(
		xml.underscorify() + '_client_h',
		input: xml,
		output: '@BASENAME@-client-protocol.h',
		command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
	)
endforeach

lib_sommelier_protos = static_library(
	'sommelier_protos',
	wl_protos_src + wl_protos_headers,
	dependencies: wayland_client.partial_dependency(compile_args: true),
)

sommelier_protos = declare_dependency(
	link_with: lib_sommelier_protos,
	sources: wl_protos_headers,
)

