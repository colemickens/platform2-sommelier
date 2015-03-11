{
  'targets': [
    {
      'target_name': 'libprotobinder',
      'type': 'shared_library',
      'sources': [
        'binder_manager.cc',
        'parcel.cc',
        'binder_proxy.cc',
        'binder_host.cc',
        'ibinder.cc',
        'iservice_manager.cc',
        'binder_proxy_interface_base.cc',
      ],
    },
  ],
}
