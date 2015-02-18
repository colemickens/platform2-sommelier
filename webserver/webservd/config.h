// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_CONFIG_H_
#define WEBSERVER_WEBSERVD_CONFIG_H_

#include <map>
#include <string>

#include <base/files/file_path.h>
#include <chromeos/errors/error.h>
#include <chromeos/secure_blob.h>

namespace webservd {

// This class contains global server configuration.
struct Config final {
 public:
  // Configuration of one specific protocol handler.
  struct ProtocolHandler final {
    ~ProtocolHandler();
    // Port to use.
    uint16_t port{0};
    // Specifies whether the handler is for HTTPS (true) or HTTP (false).
    bool use_tls{false};
    // Interface name to use if the protocol handler should work only on
    // particular network interface. If empty, the TCP socket will be open
    // on the specified port for all network interfaces.
    std::string interface_name;
    // For HTTPS handlers, these specify the certificates/private keys used
    // during TLS handshake and communication session. For HTTP protocol
    // handlers these fields are not used and are empty.
    chromeos::SecureBlob private_key;
    chromeos::Blob certificate;
    chromeos::Blob certificate_fingerprint;

    // Custom socket created for protocol handlers that are bound to specific
    // network interfaces only. SO_BINDTODEVICE option on a socket does exactly
    // what is required but it needs root access. So we create those sockets
    // before we drop privileges.
    int socket_fd{-1};
  };

  // List of all registered protocol handlers for the web server.
  std::map<std::string, ProtocolHandler> protocol_handlers;

  // Specifies whether additional debugging information should be included.
  // When set, this turns out additional diagnostic logging in libmicrohttpd as
  // well as includes additional information in error responses delivered to
  // HTTP clients.
  bool use_debug{false};
};

// Initializes the config with default preset settings (two handlers, one for
// HTTP on port 80 and one for HTTPS on port 443).
void LoadDefaultConfig(Config* config);

// Loads server configuration form specified file. The file is expected
// to exist and contain a valid configuration in JSON format.
// Returns false on error (whether opening/reading the file or parsing JSON
// content).
bool LoadConfigFromFile(const base::FilePath& json_file_path, Config* config);

// Loads the configuration from a string containing JSON data.
// In case of parsing or configuration validation errors, returns false and
// specifies the reason for the failure in |error| object.
bool LoadConfigFromString(const std::string& config_json,
                          Config* config,
                          chromeos::ErrorPtr* error);

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_CONFIG_H_
