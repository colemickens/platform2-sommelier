// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_FAKE_USERDB_H_
#define SOMA_LIB_SOMA_FAKE_USERDB_H_

#include <sys/types.h>

#include <map>
#include <string>

#include <base/macros.h>
#include "soma/lib/soma/userdb.h"

namespace soma {
namespace parser {

class FakeUserdb : public UserdbInterface {
 public:
  FakeUserdb();
  ~FakeUserdb() override;

  void set_whitelisted_namespace(const std::string& ns) {
    whitelisted_namespace_ = ns;
  }

  void set_user_mapping(const std::string& user, uid_t uid) {
    user_mappings_[user] = uid;
  }

  void set_group_mapping(const std::string& group, gid_t gid) {
    group_mappings_[group] = gid;
  }

  // UserdbInterface
  bool ResolveUser(const std::string& user, uid_t* uid) override;
  bool ResolveGroup(const std::string& group, gid_t* gid) override;

 private:
  // If set, all users or groups in this namespace will have IDs
  std::string whitelisted_namespace_;
  uid_t next_uid_;
  gid_t next_gid_;

  std::map<std::string, uid_t> user_mappings_;
  std::map<std::string, gid_t> group_mappings_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserdb);
};

}  // namespace parser
}  // namespace soma
#endif  // SOMA_LIB_SOMA_FAKE_USERDB_H_
