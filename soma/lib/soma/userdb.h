// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_USERDB_H_
#define SOMA_LIB_SOMA_USERDB_H_

#include <sys/types.h>

#include <string>

namespace soma {
namespace parser {

// This interface wraps some stdlib-type calls for the purposes of faking.
class UserdbInterface {
 public:
  virtual ~UserdbInterface() = default;

  // Uses getpwnam_r/getgpnam_r to resolve the given user or group.
  // Returns true and populates uid/gid if resolution is possible.
  virtual bool ResolveUser(const std::string& user, uid_t* uid) = 0;
  virtual bool ResolveGroup(const std::string& group, gid_t* gid) = 0;
};

class Userdb : public UserdbInterface {
 public:
  Userdb() = default;
  ~Userdb() override = default;

  // UserdbInterface
  bool ResolveUser(const std::string& user, uid_t* uid) override;
  bool ResolveGroup(const std::string& group, gid_t* gid) override;
};

}  // namespace parser
}  // namespace soma
#endif  // SOMA_LIB_SOMA_USERDB_H_
