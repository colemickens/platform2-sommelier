// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_CONFIG_VALIDATOR_H_
#define KERBEROS_CONFIG_VALIDATOR_H_

#include <string>
#include <unordered_set>

#include <base/macros.h>

#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace kerberos {

// Verifies that only whitelisted configuration options are used in a Kerberos
// configuration. The Kerberos daemon does not allow all options for security
// reasons. Also performs basic syntax checks and returns more useful error
// information than "You screwed up your config, screw you!"
class ConfigValidator {
 public:
  ConfigValidator();

  // Checks the Kerberos configuration |krb5conf|. If the config cannot be
  // parsed or a non-whitelisted option is used, returns a message with proper
  // error code and the 0-based line index where the error occurred. If the
  // config was validated successfully, returns a message with code set to
  // |CONFIG_ERROR_NONE|.
  ConfigErrorInfo Validate(const std::string& krb5conf) const;

 private:
  bool IsKeySupported(const std::string& key,
                      const std::string& section,
                      int group_level) const;

  // Note: std::hash<const char*> hashes pointers, not the strings!
  struct StrHash {
    size_t operator()(const char* str) const;
  };

  // Equivalent to strcmp(a,b) == 0.
  struct StrEquals {
    bool operator()(const char* a, const char* b) const;
  };

  using StringHashSet = std::unordered_set<const char*, StrHash, StrEquals>;
  StringHashSet libdefaults_whitelist_;
  StringHashSet realms_whitelist_;
  StringHashSet section_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(ConfigValidator);
};

}  // namespace kerberos

#endif  // KERBEROS_CONFIG_VALIDATOR_H_
