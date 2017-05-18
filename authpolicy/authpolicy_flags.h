// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_AUTHPOLICY_FLAGS_H_
#define AUTHPOLICY_AUTHPOLICY_FLAGS_H_

#include <string>

#include "bindings/authpolicy_containers.pb.h"

namespace base {
class FilePath;
}

namespace authpolicy {

// Serializes |flags| to a base 64 string.
std::string SerializeFlags(const protos::DebugFlags& flags);

// Deserializes |flags| from a base 64 string |proto_encoded|.
bool DeserializeFlags(const std::string& proto_encoded,
                      protos::DebugFlags* flags);

// Simple class for managing debug flags. See protos::DebugFlags for a
// description of available flags.
//
// Flags are loaded from a JSON file at /etc/authpolicyd_flags (see
// Path::DEBUG_FLAGS).
//
// Example:
// echo '{"log_commands":true,"log_command_output":true,"net_log_level":"10"}' \
//      > /etc/authpolicyd_flags
// turns on verbose logging of net commands.
class AuthPolicyFlags {
 public:
  // Defines 4 sets of flag levels for SetDefaults().
  enum DefaultLevel {
    kQuiet,     // All flags off (default).
    kTaciturn,  // A few logs only with important stats.
    kChatty,    // More verbose logs, low debug level.
    kVerbose,   // Log everything except seccomp, high debug level. Seccomp
                // failure logging whitelists a few syscalls and hence has a
                // negative impact on security.
  };

  // Applies defaults according to |default_level|.
  void SetDefaults(DefaultLevel default_level);

  // Loads flags from the JSON file at |path|. Returns false if the file does
  // not exist. Prints warnings and errors. Misspelled/unknown settings are
  // ignored. Malformed files (invalid JSON) are ignored altogether.
  bool LoadFromJsonFile(const base::FilePath& path);

  // Loads flags from the JSON string |flags_json|. Prints warnings and errors.
  // Misspelled/unknown settings are ignored. Malformed strings (invalid JSON)
  // are ignored altogether.
  void LoadFromJsonString(const std::string& flags_json);

  // Prints out all flags.
  void Dump() const;

  // Gets the underlying protobuf with all flags.
  const protos::DebugFlags& Get() const { return flags_; }

 private:
  protos::DebugFlags flags_;
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_AUTHPOLICY_FLAGS_H_
