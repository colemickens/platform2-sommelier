{
  'targets': [
    {
      'target_name': 'weave',
      'type': 'executable',
      'cflags': ['-pthread'],
      'sources': [
        'main.cc',
        'file_config_store.cc',
        'event_task_runner.cc',
        'curl_http_client.cc',
        'network_manager.cc',
        'avahi_client.cc',
        'event_http_server.cc',
      ],
      'dependencies': [
        '../../libweave_standalone.gyp:libweave',
      ],
      'libraries': [
        '-levent',
        '-lcrypto',
        '-lexpat',
        '-lcurl',
        '-lpthread',
        '-lssl',
        '-lavahi-common',
        '-lavahi-client',
        '-levent_openssl',
      ]
    }
  ]
}
