// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ENTRY_
#define SHILL_ENTRY_

#include <string>

#include "shill/refptr_types.h"

namespace shill {

class Entry : public base::RefCounted<Entry> {
 public:
  struct EapCredentials {
    EapCredentials() : use_system_cas(false) {}
    std::string identity;
    std::string eap;
    std::string inner_eap;
    std::string anonymous_identity;
    std::string client_cert;
    std::string cert_id;
    std::string private_key;
    std::string private_key_password;
    std::string key_id;
    std::string ca_cert;
    std::string ca_cert_id;
    bool use_system_cas;
    std::string pin;
    std::string password;
    std::string key_management;
  };

  explicit Entry(const std::string &profile)
      : profile_name(profile),
        auto_connect(false),
        hidden_ssid(false),
        save_credentials(false) {
  }
  virtual ~Entry() {}

  // Properties queryable via RPC.
  std::string profile_name;
  bool auto_connect;
  std::string failure;
  std::string modified;
  std::string mode;           // wifi services only.
  std::string security;       // wifi services only.
  bool hidden_ssid;    // wifi services only.
  std::string provider_name;  // VPN services only.
  std::string provider_host;  // VPN services only.
  std::string provider_type;  // VPN services only.
  std::string vpn_domain;     // VPN services only.

  bool save_credentials;
  EapCredentials eap;         // Only saved if |save_credentials| is true.

  // Properties not queryable via RPC.
  ServiceRefPtr service;
 private:
  DISALLOW_COPY_AND_ASSIGN(Entry);
};

}  // namespace shill

#endif  // SHILL_ENTRY_
