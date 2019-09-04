# Caution!: GYP to GN migration is happening. If you update this file, please
# update common-mk/external_dependencies/BUILD.gn too accordingly.
{
  'targets': [
    {
      'target_name': 'modemmanager-dbus-adaptors',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'adaptor',
        'xml2cpp_in_dir': '<(sysroot)/usr/share/dbus-1/interfaces/',
        'xml2cpp_out_dir': 'include/dbus_adaptors',
      },
      'sources': [
        '<(xml2cpp_in_dir)/mm-mobile-error.xml',
        '<(xml2cpp_in_dir)/mm-serial-error.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Cdma.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Firmware.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Gsm.Card.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Gsm.Contacts.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Gsm.Network.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Gsm.SMS.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Gsm.Ussd.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Gsm.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.Simple.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.Modem.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Bearer.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Modem.Location.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Modem.Modem3gpp.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Modem.ModemCdma.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Modem.Simple.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Modem.Time.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Modem.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.Sim.xml',
        '<(xml2cpp_in_dir)/org.freedesktop.ModemManager1.xml',
      ],
      'includes': ['xml2cpp.gypi'],
    },
    {
      'target_name': 'dbus-proxies',
      'type': 'none',
      'variables': {
        'xml2cpp_type': 'proxy',
        'xml2cpp_in_dir': '<(sysroot)/usr/share/dbus-1/interfaces/',
        'xml2cpp_out_dir': 'include/dbus_proxies',
      },
      'sources': [
        '<(xml2cpp_in_dir)/org.freedesktop.DBus.Properties.xml',
      ],
      'includes': ['xml2cpp.gypi'],
    },
    {
      'target_name': 'policy-protos',
      'type': 'static_library',
      # policy-protos.a is used by a shared_libary object:
      #   - https://bugs.chromium.org/p/chromium/issues/detail?id=715795
      # Build it with '-fPIC' instead of '-fPIE'.
      'cflags!': ['-fPIE'],
      'cflags': ['-fPIC'],
      'variables': {
        'proto_in_dir': '<(sysroot)/usr/include/proto',
        'proto_out_dir': 'include/bindings',
      },
      'sources': [
        '<(proto_in_dir)/chrome_device_policy.proto',
        '<(proto_in_dir)/chrome_extension_policy.proto',
        '<(proto_in_dir)/device_management_backend.proto',
      ],
      'includes': ['protoc.gypi'],
    },
    {
      'target_name': 'install_attributes-proto',
      'type': 'static_library',
      # install_attributes-proto.a is used by a shared_libary
      # object, so we need to build it with '-fPIC' instead of '-fPIE'.
      'cflags!': ['-fPIE'],
      'cflags': ['-fPIC'],
      'variables': {
        'proto_in_dir': '<(sysroot)/usr/include/proto',
        'proto_out_dir': 'include/bindings',
      },
      'sources': [
        '<(proto_in_dir)/install_attributes.proto',
      ],
      'includes': ['protoc.gypi'],
    },
  ],
}
